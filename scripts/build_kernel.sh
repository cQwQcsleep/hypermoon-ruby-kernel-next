#!/usr/bin/env bash
# Build the ruby (Redmi Note 12 Pro 5G / MT6877) 4.19 kernel.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VARIANT="${1:-${VARIANT:-vanilla}}"
case "$VARIANT" in
	vanilla|ksu) ;;
	*)
		echo "error: unknown variant '$VARIANT' (use vanilla or ksu)" >&2
		exit 1
		;;
esac

OUT="${OUT:-$ROOT/out-$VARIANT}"
JOBS="${JOBS:-$(nproc)}"
LLVM_DIR="${LLVM_DIR:-/usr/lib/llvm-17}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"

CC="${CC:-$LLVM_DIR/bin/clang}"
LD="${LD:-$LLVM_DIR/bin/ld.lld}"
AR="${AR:-$LLVM_DIR/bin/llvm-ar}"
NM="${NM:-$LLVM_DIR/bin/llvm-nm}"
OBJCOPY="${OBJCOPY:-$LLVM_DIR/bin/llvm-objcopy}"
OBJDUMP="${OBJDUMP:-$LLVM_DIR/bin/llvm-objdump}"
READELF="${READELF:-$LLVM_DIR/bin/llvm-readelf}"
OBJSIZE="${OBJSIZE:-$LLVM_DIR/bin/llvm-size}"
STRIP="${STRIP:-$LLVM_DIR/bin/llvm-strip}"

for tool in "$CC" "$LD" "$AR" "$NM" "$OBJCOPY" "$OBJDUMP" "$READELF" "$OBJSIZE" "$STRIP"; do
	if [ ! -x "$tool" ]; then
		echo "error: missing tool: $tool" >&2
		exit 1
	fi
done

if ! command -v "${CROSS_COMPILE}gcc" >/dev/null 2>&1; then
	echo "error: missing ${CROSS_COMPILE}gcc (aarch64 cross compiler)" >&2
	exit 1
fi
if ! command -v arm-linux-gnueabi-gcc >/dev/null 2>&1; then
	echo "error: missing arm-linux-gnueabi-gcc (32-bit vDSO toolchain)" >&2
	exit 1
fi

if [ -z "$OUT" ] || [ "$OUT" = "/" ]; then
	echo "error: refusing to use OUT=$OUT" >&2
	exit 1
fi
rm -rf "$OUT"
mkdir -p "$OUT"

# 4.19 links the 32-bit compat vDSO with ld.lld, which does not pull in
# libgcc automatically. Append the ARM libgcc so __aeabi_* helpers resolve.
ARM32_LD="$OUT/arm32-ld"
cat >"$ARM32_LD" <<EOF
#!/bin/sh
exec "$LD" "\$@" "\$(arm-linux-gnueabi-gcc -print-libgcc-file-name)"
EOF
chmod +x "$ARM32_LD"

MAKE_ARGS=(
	O="$OUT"
	ARCH=arm64
	CC="$CC"
	LLVM=1
	LLVM_IAS=1
	LD="$LD"
	LD_COMPAT="$ARM32_LD"
	LLVM_AR="$AR"
	LLVM_NM="$NM"
	AR="$AR"
	NM="$NM"
	OBJCOPY="$OBJCOPY"
	OBJDUMP="$OBJDUMP"
	READELF="$READELF"
	OBJSIZE="$OBJSIZE"
	STRIP="$STRIP"
	CROSS_COMPILE="$CROSS_COMPILE"
)

make "${MAKE_ARGS[@]}" ruby_defconfig
scripts/kconfig/merge_config.sh -m -O "$OUT" \
	"$OUT/.config" arch/arm64/configs/vendor/ruby_features.config >/dev/null 2>&1
if [ "$VARIANT" = "ksu" ]; then
	scripts/kconfig/merge_config.sh -m -O "$OUT" \
		"$OUT/.config" arch/arm64/configs/vendor/ruby_ksu.config >/dev/null 2>&1
fi
make "${MAKE_ARGS[@]}" olddefconfig
make -j"$JOBS" "${MAKE_ARGS[@]}" Image.gz-dtb

echo "kernel: $OUT/arch/arm64/boot/Image.gz-dtb"
