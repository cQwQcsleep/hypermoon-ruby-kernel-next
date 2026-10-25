#!/usr/bin/env bash
# Build the ruby (Redmi Note 12 Pro 5G / MT6877) 4.19 kernel.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

VARIANT="${1:-${VARIANT:-vanilla}}"
# Every manager ships its own UAPI revision and compares it against the value
# the kernel reports in GET_INFO, so build one kernel per manager with the
# matching value. Released values (checked against each upstream tag):
#   KernelSU v3.3.1            -> 5
#   KernelSU-Next v3.4.2       -> 5
#   SukiSU-Ultra v4.2.1-r2     -> 6
#   ReSukiSU v4.2.0-rc3        -> 6
#   MKSU (5ec1cff) v9.9.9      -> 2
#   RKSU (rsuntk) v3.2.2-10    -> no UAPI check, kernel version >= 32377
case "$VARIANT" in
	vanilla) ;;
	ksu|ksu-resukisu)
		KSU_UAPI_VERSION_OVERRIDE=6
		KSU_TAG_NAME_OVERRIDE=v4.2.0-rc3
		KSU_COMMIT_SHA_OVERRIDE=639e2e36
		;;
	ksu-sukisu)
		KSU_UAPI_VERSION_OVERRIDE=6
		KSU_TAG_NAME_OVERRIDE=v4.2.1-r2
		;;
	ksu-official)
		KSU_UAPI_VERSION_OVERRIDE=5
		KSU_TAG_NAME_OVERRIDE=v3.3.1
		;;
	ksu-next)
		KSU_UAPI_VERSION_OVERRIDE=5
		KSU_TAG_NAME_OVERRIDE=v3.4.2
		;;
	ksu-mksu)
		KSU_UAPI_VERSION_OVERRIDE=2
		KSU_TAG_NAME_OVERRIDE=v9.9.9
		;;
	ksu-rksu)
		KSU_UAPI_VERSION_OVERRIDE=6
		KSU_TAG_NAME_OVERRIDE=v3.2.2-10
		;;
	*)
		echo "error: unknown variant '$VARIANT'" >&2
		echo "       use vanilla, ksu, ksu-official, ksu-next, ksu-sukisu," >&2
		echo "       ksu-resukisu, ksu-mksu or ksu-rksu" >&2
		exit 1
		;;
esac
export KSU_UAPI_VERSION_OVERRIDE KSU_TAG_NAME_OVERRIDE KSU_COMMIT_SHA_OVERRIDE

OUT="${OUT:-$ROOT/out-$VARIANT}"
JOBS="${JOBS:-$(nproc)}"
# HyperMoon 1.0.2 is built with the AOSP clang r383902 prebuilt (11.0.1) and
# that combination is the one proven to boot on ruby; prefer it when present.
if [ -z "${LLVM_DIR:-}" ]; then
	for d in /opt/clang-r383902 "$HOME/toolchain/clang" "$HOME/clang-r383902" /usr/lib/llvm-17; do
		[ -x "$d/bin/clang" ] && { LLVM_DIR="$d"; break; }
	done
fi
LLVM_DIR="${LLVM_DIR:-/usr/lib/llvm-17}"
CROSS_COMPILE="${CROSS_COMPILE:-aarch64-linux-gnu-}"

# Do not embed the build machine/user name in the kernel version banner
KBUILD_BUILD_USER="${KBUILD_BUILD_USER:-builder}"
KBUILD_BUILD_HOST="${KBUILD_BUILD_HOST:-buildhost}"
export KBUILD_BUILD_USER KBUILD_BUILD_HOST

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
	KBUILD_BUILD_USER="$KBUILD_BUILD_USER"
	KBUILD_BUILD_HOST="$KBUILD_BUILD_HOST"
)

make "${MAKE_ARGS[@]}" ruby_defconfig
scripts/kconfig/merge_config.sh -m -O "$OUT" \
	"$OUT/.config" arch/arm64/configs/vendor/ruby_features.config >/dev/null 2>&1
if [ "$VARIANT" != "vanilla" ]; then
	scripts/kconfig/merge_config.sh -m -O "$OUT" \
		"$OUT/.config" arch/arm64/configs/vendor/ruby_ksu.config >/dev/null 2>&1
fi
make "${MAKE_ARGS[@]}" olddefconfig
if [ "$VARIANT" != "vanilla" ]; then
	# KSU uses generated UTS/SELinux headers; build their producers first
	# so parallel clean builds cannot race them.
	make -j"$JOBS" "${MAKE_ARGS[@]}" init/
	make -j"$JOBS" "${MAKE_ARGS[@]}" security/selinux/
fi
make -j"$JOBS" "${MAKE_ARGS[@]}" Image.gz-dtb

echo "kernel: $OUT/arch/arm64/boot/Image.gz-dtb"
