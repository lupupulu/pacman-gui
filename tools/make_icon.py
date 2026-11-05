#!/usr/bin/env python3
"""生成应用图标。

用法：python3 tools/make_icon.py

输出三个文件：
  assets/icon.png       256x256  原始尺寸，供打包 .desktop / hicolor 图标用
  assets/icon-64.png     64x64   给 SDL_SetWindowIcon 用
  assets/icon-1024.png  1024x1024 给 macOS 的 .icns 用

窗口图标为什么单独出一份 64x64：X11 设置窗口图标靠 XChangeProperty 写
_NET_WM_ICON，一次请求有大小上限，256x256 的图标数据超限后会被静默丢弃
（属性变成空的，图标不显示也不报错）。64x64 在各平台都稳妥。

先在 4 倍尺寸上绘制再缩小，用超采样换抗锯齿，不依赖任何外部素材。
"""
import os
from PIL import Image, ImageDraw

ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")

MASTER = 1024       # 用于 macOS .icns 的高分辨率母版
WINDOW = 64         # 窗口图标尺寸
LINUX = 256         # Linux/桌面图标尺寸
SS = 4              # 超采样倍数
S = MASTER * SS

BG = (27, 33, 64, 255)        # 深蓝底
BORDER = (70, 88, 150, 255)   # 描边
PAC = (255, 205, 40, 255)     # 吃豆人黄
PELLET = (235, 240, 255, 255) # 豆子白


def main():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # 圆角方形底 + 内描边
    pad = int(S * 0.02)
    radius = int(S * 0.22)
    d.rounded_rectangle([pad, pad, S - 1 - pad, S - 1 - pad],
                        radius=radius, fill=BG, outline=BORDER, width=int(S * 0.025))

    # 吃豆人：嘴巴朝右，缺口 70 度
    cx, cy = S * 0.42, S * 0.5
    r = S * 0.25
    d.pieslice([cx - r, cy - r, cx + r, cy + r], start=35, end=325, fill=PAC)

    # 右前方三颗豆子，越远越小
    for px, scale in ((0.70, 0.050), (0.79, 0.042), (0.88, 0.034)):
        pr = S * scale
        x = S * px
        d.ellipse([x - pr, cy - pr, x + pr, cy + pr], fill=PELLET)

    os.makedirs(ASSETS, exist_ok=True)
    for size, name in ((LINUX, "icon.png"), (WINDOW, "icon-64.png"), (MASTER, "icon-1024.png")):
        out = os.path.join(ASSETS, name)
        img.resize((size, size), Image.LANCZOS).save(out)
        print("已生成", os.path.normpath(out), f"{size}x{size}")


if __name__ == "__main__":
    main()
