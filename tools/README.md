# tools

## `ttf_to_c.py`

把 TTF/OTF 直接转成一个可编译的 C 文件：字体字节变成一个 `const unsigned char`
数组，链接进可执行文件后运行时不再需要外部字体文件。

```sh
python3 tools/ttf_to_c.py assets/PacmanPixel-ASCII.ttf \
  -o build/pacman_pixel_font.c \
  --symbol pacman_pixel_ttf
```

会生成：

```c
const unsigned char pacman_pixel_ttf[];
const size_t pacman_pixel_ttf_len;
```

本项目不自动生成 `.h`；把这两行声明加到手写源码里（例如 `sdlgui.c`）：

```c
extern const unsigned char pacman_pixel_ttf[];
extern const size_t pacman_pixel_ttf_len;
```

在 C 里通过 SDL3_ttf 打开：

```c
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

SDL_IOStream *io = SDL_IOFromConstMem(pacman_pixel_ttf,
                                      pacman_pixel_ttf_len);
TTF_Font *font = TTF_OpenFontIO(io, true, 20.0f);
/* closeio=true：TTF_CloseFont() 时会一并关闭 io */
```

常用参数：

| 参数 | 说明 |
| --- | --- |
| `input` | 输入 `.ttf` / `.otf` |
| `-o, --output` | 输出的 `.c`，默认和输入同目录、同名 |
| `--header` | 可选：额外生成带 `extern` 声明的 `.h`；本项目不使用 |
| `--symbol` | 数组符号名，默认由文件名推导 |
| `--line-width` | 每行字节数，默认 12 |
| `--quiet` | 只输出错误 |

生成结果按内容比较：内容没变时不改 mtime，避免无意义的重编译。
生成物放在 `build/` 下，不进入源码树；`make clean` 会一起清掉。

`Makefile` 里有两条相关规则：

- `build/pacman_pixel_font.c` 由 TTF 和 `tools/ttf_to_c.py` 生成；
- `build/obj/pacman_pixel_font.o` 编译后再加入 `OBJS`，参与最终链接。
- 不生成 `.h`；符号声明直接写在使用它的手写源码里。

普通 `make` 会在 TTF 或脚本变化时自动重新生成；也可以显式运行：

```sh
make font
```

构建机需要 Python 3.10+（只在生成嵌入源时用，生成后就是普通 C 文件）。

### 当前项目里的结果

- 字体：`assets/PacmanPixel-ASCII.ttf`（5,292 字节）
- 构建时生成：`build/pacman_pixel_font.c`（约 33 KB 文本）
- 可执行文件中的符号：`pacman_pixel_ttf`（5,292 字节）、`pacman_pixel_ttf_len`
- 已验证：`TTF_OpenFontIO(SDL_IOFromConstMem(...))` 可直接渲染
- `sdlgui_init()` 会把该字体以 20 pt 装入默认主题的 `theme.fonts[0]`

## `gen_ascii_font.py`

生成 `assets/PacmanPixel-ASCII.ttf` 本身（5×7 等宽像素体，仅 ASCII）。
需要 Python 3.10+ 和 fontTools：

```sh
python3 tools/gen_ascii_font.py
```

## `make_icon.py`

项目图标的辅助脚本。
