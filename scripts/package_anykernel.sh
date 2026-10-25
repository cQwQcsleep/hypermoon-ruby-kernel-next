#!/usr/bin/env bash
# Package Image.gz-dtb into an AnyKernel3 flashable zip.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VARIANT="${1:-${VARIANT:-vanilla}}"
case "$VARIANT" in
	vanilla) KSU_NAME="" ;;
	ksu|ksu-resukisu) KSU_NAME="ReSukiSU" ;;
	ksu-sukisu) KSU_NAME="SukiSU-Ultra" ;;
	ksu-official) KSU_NAME="KernelSU" ;;
	ksu-next) KSU_NAME="KernelSU-Next" ;;
	ksu-mksu) KSU_NAME="MKSU" ;;
	ksu-rksu) KSU_NAME="RKSU" ;;
	*)
		echo "error: unknown variant '$VARIANT'" >&2
		exit 1
		;;
esac
OUT="${OUT:-$ROOT/out-$VARIANT}"
DIST="${DIST:-$ROOT/dist}"
# HyperMoon ships the gzip-only Image.gz and lets AnyKernel3 keep the stock
# boot dtb, so package the same payload name for an identical flash path.
IMAGE_GZ="$OUT/arch/arm64/boot/Image.gz"
IMAGE_DTB="$OUT/arch/arm64/boot/Image.gz-dtb"
if [ -f "$IMAGE_GZ" ]; then
	IMAGE="$IMAGE_GZ"
	PAYLOAD="Image.gz"
elif [ -f "$IMAGE_DTB" ]; then
	IMAGE="$IMAGE_DTB"
	PAYLOAD="Image.gz-dtb"
else
	echo "error: missing kernel image in $OUT/arch/arm64/boot" >&2
	echo "       (expected Image.gz or Image.gz-dtb; run scripts/build_kernel.sh first)" >&2
	exit 1
fi
TEMPLATE="${ANY_KERNEL3_DIR:-$ROOT/anykernel3}"

if [ ! -f "$TEMPLATE/anykernel.sh" ]; then
	echo "error: missing vendored AnyKernel3 template at $TEMPLATE" >&2
	exit 1
fi

mkdir -p "$DIST"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp -a "$TEMPLATE/." "$WORK/"
rm -rf "$WORK/.git" "$WORK/.github"
cp "$ROOT/anykernel.sh" "$WORK/anykernel.sh"
if [ -n "$KSU_NAME" ]; then
	sed -i "s|kernel.string=.*|kernel.string=HyperMoon Ruby Next (KSU + SUSFS / $KSU_NAME)|" "$WORK/anykernel.sh"
fi
cp "$IMAGE" "$WORK/$PAYLOAD"

VERSION="4.19.325"
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
SUFFIX=""
if [ -n "$KSU_NAME" ]; then
	SUFFIX="-$VARIANT"
fi
ZIP="$DIST/hypermoon-ruby-${VERSION}-${COMMIT}${SUFFIX}.zip"

rm -f "$ZIP"
(
	cd "$WORK"
	zip -r9 "$ZIP" .
)

echo "package: $ZIP"
