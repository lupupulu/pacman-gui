#!/bin/sh
# 用 macOS 自带的 sips / iconutil 把一张 PNG 转成 .icns。
# 用法：sh tools/make_icns.sh <source.png> <iconset-dir> <out.icns>
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <source.png> <iconset-dir> <out.icns>" >&2
    exit 2
fi

SOURCE=$1
ICONSET=$2
OUTPUT=$3

rm -rf "$ICONSET"
mkdir -p "$ICONSET"

for size in 16 32 128 256 512; do
    double=$((size * 2))
    sips -z "$size" "$size" "$SOURCE" --out "$ICONSET/icon_${size}x${size}.png" >/dev/null
    sips -z "$double" "$double" "$SOURCE" --out "$ICONSET/icon_${size}x${size}@2x.png" >/dev/null
done

iconutil -c icns "$ICONSET" -o "$OUTPUT"
rm -rf "$ICONSET"
