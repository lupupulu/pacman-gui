# pacman-gui — SDL3 空白窗口模板

一个可以立刻编译运行的最小 SDL3 骨架：初始化 + 开窗口 + 渲染器 + 事件循环 + 清理。
运行后得到一个 1024×720 的可缩放深色空窗口，ESC 或关闭按钮退出。
后续的界面与逻辑按 `src/main.c` 里的 TODO 往下写即可。

## 依赖

- C17 编译器（gcc / clang）
- SDL3 开发包（本机 3.4.18）、pkg-config、make
- Python 3.10+（构建时运行 `tools/ttf_to_c.py`，把字体嵌成 C）

Arch Linux：

```sh
sudo pacman -S sdl3
```

## 编译与运行

```sh
make          # 生成 build/pacman-gui
make run      # 编译并运行
make debug    # ASan/UBSan 版本，查内存问题
make font     # 重新生成嵌入字体的 build/pacman_pixel_font.c
make clean    # 清理
```

## 目录结构

```
Makefile       构建规则（pkg-config 自动取 SDL3 的编译/链接参数）
src/main.c     全部代码：初始化、主循环、事件、绘制、退出
build/pacman_pixel_font.c  构建时生成的嵌入字体源（已被 .gitignore 忽略）
tools/         字体/图标生成脚本
build/         构建产物（已被 .gitignore 忽略）
```

`Makefile` 用 `$(wildcard src/*.c)` 收集源文件，之后再拆出 `ui.c`、`alpm.c`
之类的模块时不用改构建规则。

## 代码要点

- SDL3 用返回值判断成败（`bool` + `SDL_GetError()`），没有 SDL2 那种 `int` 返回码。
- `SDL_Window *` / `SDL_Renderer *` 在 SDL3 里已经是不透明指针，创建时传 `&window`。
- `SDL_SetRenderVSync(renderer, 1)` 让主循环跟着显示器刷新走，否则会空转占满一个核。
- 主循环固定三段：`SDL_PollEvent` 收事件 → 更新状态 → `SDL_RenderClear` /
  绘制 / `SDL_RenderPresent`。要按时间推进动画时，用
  `SDL_GetPerformanceCounter()` 求 `dt`（秒）并夹一个上限。
- 窗口可缩放，但绘制坐标用的是像素。若想让界面随窗口等比缩放，用
  `SDL_SetRenderLogicalPresentation(renderer, 逻辑宽, 逻辑高, SDL_LOGICAL_PRESENTATION_LETTERBOX)`，
  之后所有坐标都按固定逻辑分辨率写。

## 嵌入字体

`assets/PacmanPixel-ASCII.ttf` 在构建时由 `tools/ttf_to_c.py` 转成
`build/pacman_pixel_font.c`，随后编译并自动链接进可执行文件，运行时不需要外部字体文件。
生成物只留在 `build/` 下，`make clean` 会一起清掉；修改字体后普通 `make` 会自动重新生成，
也可以显式运行 `make font`。默认主题在 `sdlgui_init()` 里把该字体以第一个字号（20 pt）打开，
存入 `theme.fonts[0]` / `theme.font_size = 1`，并在 `sdlgui_quit()` 中关闭。
具体用法和 SDL3_ttf 打开方式见 `tools/README.md`。

## 加依赖：只需改 Makefile 一行

```make
PKGS := sdl3 sdl3_ttf
```

GUI 里必然要显示文字，用 SDL3_ttf 渲染分数/包名列表的最小例子：

```c
#include <SDL3_ttf/SDL_ttf.h>

TTF_Init();
TTF_Font *font = TTF_OpenFont("/usr/share/fonts/TTF/Hack-Regular.ttf", 20);
SDL_Surface *surf = TTF_RenderText_Blended(font, "packages", 0, (SDL_Color){ 255, 255, 255, 255 });
SDL_Texture *tex  = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FRect dst = { 20, 20, (float)surf->w, (float)surf->h };
SDL_RenderTexture(renderer, tex, NULL, &dst);
```

中文需要加载含中文字形的字体（本机可用 `/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc`）。
其他常用库同理加入 `PKGS`：`sdl3_image`（贴图）。

若要做真正的 pacman 包管理器 GUI，接下来通常还要链 `libalpm`
（Arch 的包管理后端，`pacman -S pacman` 提供头文件与库；用 CMake 的
`find_package(LibAlpm)` 或手工加 `-lalpm` 引入），并在单独线程里跑
数据库读取与安装操作，避免阻塞渲染循环。
