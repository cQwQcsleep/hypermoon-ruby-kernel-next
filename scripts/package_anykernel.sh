#!/usr/bin/env bash
# Package Image.gz-dtb into an AnyKernel3 flashable zip.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VARIANT="${1:-${VARIANT:-vanilla}}"
case "$VARIANT" in
	vanilla|ksu) ;;
	*)
		echo "error: unknown variant '$VARIANT' (use vanilla or ksu)" >&2
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
TEMPLATE="${ANY_KERNEL3_DIR:-$ROOT/.cache/AnyKernel3}"

if [ ! -f "$TEMPLATE/anykernel.sh" ]; then
	mkdir -p "$(dirname "$TEMPLATE")"
	curl -L --retry 3 --fail \
		"https://github.com/osm0sis/AnyKernel3/archive/refs/heads/master.tar.gz" \
		-o "$ROOT/.cache/anykernel3.tar.gz"
	rm -rf "$TEMPLATE"
	mkdir -p "$TEMPLATE"
	tar -xzf "$ROOT/.cache/anykernel3.tar.gz" -C "$TEMPLATE" --strip-components=1
fi

mkdir -p "$DIST"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp -a "$TEMPLATE/." "$WORK/"
rm -rf "$WORK/.git" "$WORK/.github" "$WORK/README.md"
cp "$ROOT/anykernel.sh" "$WORK/anykernel.sh"
if [ "$VARIANT" = "ksu" ]; then
	sed -i 's/kernel.string=.*/kernel.string=HyperMoon Ruby Next (KSU + SUSFS)/' "$WORK/anykernel.sh"
fi
cp "$IMAGE" "$WORK/$PAYLOAD"

VERSION="4.19.325-cip135"
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
SUFFIX=""
if [ "$VARIANT" = "ksu" ]; then
	SUFFIX="-ksu"
fi
ZIP="$DIST/hypermoon-ruby-${VERSION}-${COMMIT}${SUFFIX}.zip"

rm -f "$ZIP"
(
	cd "$WORK"
	zip -r9 "$ZIP" . -x '*placeholder'
)

echo "package: $ZIP"
