#!/bin/sh
# 将默认主题的 SVG 图标按 64x64 渲染为同名 PNG。
# 源 SVG 保留在 assets/Default/icon/ 下，PNG 只作为运行时资源。
set -eu
cd "$(dirname "$0")/.."

for svg in assets/Default/icon/*.svg; do
    [ -e "$svg" ] || continue
    png=${svg%.svg}.png
    rsvg-convert -w 64 -h 64 -o "$png" "$svg"
    echo "rendered ${png}"
done
