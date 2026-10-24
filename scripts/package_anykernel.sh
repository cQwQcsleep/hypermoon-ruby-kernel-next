#!/usr/bin/env bash
# Package Image.gz-dtb into an AnyKernel3 flashable zip.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${OUT:-$ROOT/out}"
DIST="${DIST:-$ROOT/dist}"
IMAGE="$OUT/arch/arm64/boot/Image.gz-dtb"
TEMPLATE="${ANY_KERNEL3_DIR:-$ROOT/.cache/AnyKernel3}"

if [ ! -f "$IMAGE" ]; then
	echo "error: missing $IMAGE (run scripts/build_kernel.sh first)" >&2
	exit 1
fi

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
rm -rf "$WORK/.git" "$WORK/README.md"
cp "$ROOT/anykernel.sh" "$WORK/anykernel.sh"
cp "$IMAGE" "$WORK/Image.gz-dtb"

VERSION="$(make -s -C "$ROOT" kernelversion 2>/dev/null || echo 4.19)"
COMMIT="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
ZIP="$DIST/hypermoon-ruby-${VERSION}-${COMMIT}.zip"

(
	cd "$WORK"
	zip -r9 "$ZIP" . -x '*placeholder'
)

echo "package: $ZIP"
