# pacman-gui — SDL3 桌面骨架（Linux / macOS）

一个可以立刻编译运行的最小 SDL3 骨架：初始化 + 开窗口 + 渲染器 + 事件循环 + 清理。
运行后得到一个 640×480 的可缩放窗口，ESC、Cmd+Q 或关闭按钮退出。
后续的界面与逻辑按 `src/main.c` 里的 TODO 往下写即可。

## 依赖

- C99 编译器（gcc / clang）
- SDL3 + SDL3_ttf 开发包
- make
- Python 3.9+（构建时运行 `tools/ttf_to_c.py`，把字体嵌成 C）
- 字体解析：macOS 用系统 CoreText；Linux 用 fontconfig
- gettext（可选，仅用于本地化）

Arch Linux：

```sh
sudo pacman -S sdl3 sdl3_ttf fontconfig
```

macOS（Homebrew）：

```sh
brew install sdl3 sdl3_ttf        # 必需
brew install pkgconf gettext      # 可选：pkg-config；gettext 用于本地化
```

macOS 不需要 fontconfig：`src/sdlgui_fontlookup.c` 直接调用系统 CoreText，
还能把 `monospace` / `sans-serif` / `serif` 这类通用族名映射到 Menlo、SF Pro、
Times New Roman。没有 gettext 时 `_()` 会退化成英文原文，程序照常编译运行。

## 编译与运行

```sh
make          # 生成 build/pacman-gui
make run      # 编译并运行
make debug    # ASan/UBSan 版本，查内存问题
make app      # macOS：生成 build/pacman-gui.app
make run-app  # macOS：生成并用 Finder 打开 .app
make font     # 重新生成嵌入字体的 build/pacman_pixel_font.c
make clean    # 清理
```

`make` 在 macOS 上没有装 pkg-config 也能工作：Makefile 会退回
`$(brew --prefix)` 下的 `include/` 和 `lib/`；装了 pkg-config 则自动使用
`sdl3` 和 `sdl3-ttf`（或 `SDL3_ttf`）模块。

macOS 上 `make app` 会生成标准应用包：

```
build/pacman-gui.app/Contents/
├── Info.plist
├── MacOS/pacman-gui
└── Resources/
    ├── assets/           主题配置与图标
    └── pacman-gui.icns   由 assets/icon-1024.png 生成
```

Dock 图标、菜单栏名称和高分屏支持都来自这个包；直接运行
`build/pacman-gui` 时系统仍会给它一个普通窗口，但不会有自定义图标。

这个 `.app` 默认仍然链接 Homebrew 里的 SDL3 / SDL3_ttf / gettext，适合在本机
开发和测试。要分发到没有 Homebrew 的机器，需要用 `dylibbundler` 或
`install_name_tool` 把依赖的 `.dylib` 放进 `Contents/Frameworks/` 并改写
install name，然后做正式签名/公证。

## VS Code / clangd

VS Code 的 C/C++ 扩展不会读取 Makefile 里的 `-I`，所以终端能编译时，
编辑器里仍然可能对 `<SDL3/SDL.h>` 报“找不到头文件”。

仓库里的 `.vscode/c_cpp_properties.json` 已经补上：

- `/opt/homebrew/include`（Apple Silicon）和 `/usr/local/include`（Intel）
- `src/`、`src/CDS/`
- `SDLGUI_USE_CORETEXT=1`、`cStandard: c99`

Intel Mac 把 `intelliSenseMode` 从 `macos-clang-arm64` 改成 `macos-clang-x64`。
改完后在命令面板执行 **C/C++: Reset IntelliSense Database**，或者重新打开窗口。

如果使用 clangd，先生成编译数据库：

```sh
make compdb   # 内部会 make clean，然后用 clang -MJ 重建并合并
```

根目录的 `compile_commands.json` 会被 clangd 自动读取；之后执行
**clangd: Restart language server**。`make clean` 会删掉这个文件，新增源文件后
重新运行 `make compdb` 即可。

也可以直接在终端查看真实参数：

```sh
pkg-config --cflags sdl3 sdl3-ttf
```

## 目录结构

```
Makefile       构建规则（macOS 用 Homebrew 前缀 + CoreText，Linux 用 pkg-config + fontconfig）
src/main.c     程序入口与主循环
src/sdlgui.c   窗口、鼠标、布局、绘制与高 DPI 处理
src/sdlgui.h
src/sdlgui_theme.c   主题、配色、字体缓存与 TOML 配置
src/sdlgui_theme.h
src/sdlgui_fontlookup.c/.h   字体族名 -> 字体文件（macOS CoreText / Linux fontconfig）
src/CDS/       单头文件配置/TOML 解析库
packaging/macos/Info.plist   macOS 应用包信息
build/pacman_pixel_font.c    构建时生成的嵌入字体源（已被 .gitignore 忽略）
build/lost_icon.c            构建时生成的内置 lost 图标源（已被 .gitignore 忽略）
assets/Default/icon/lost.png 默认主题自带的 lost 图标（由 gen_lost_icon.py 生成）
tools/         字体、图标、icns 与编译数据库生成脚本
assets/        主题、字体、图标
build/         构建产物（已被 .gitignore 忽略）
```

`Makefile` 用 `$(wildcard src/*.c src/*/*.c)` 收集源文件，之后再拆出 `ui.c`、
`backend.c` 之类的模块时不用改构建规则。

## 代码要点

- SDL3 用返回值判断成败（`bool` + `SDL_GetError()`），没有 SDL2 那种 `int` 返回码。
- `SDL_Window *` / `SDL_Renderer *` 在 SDL3 里已经是不透明指针，创建时传 `&window`。
- `SDL_SetRenderVSync(renderer, 1)` 让主循环跟着显示器刷新走，否则会空转占满一个核。
- 主循环固定三段：`SDL_PollEvent` 收事件 → 更新状态 → `SDL_RenderClear` /
  绘制 / `SDL_RenderPresent`。要按时间推进动画时，用
  `SDL_GetPerformanceCounter()` 求 `dt`（秒）并夹一个上限。
- 窗口创建时带 `SDL_WINDOW_HIGH_PIXEL_DENSITY`：macOS 的 Retina 显示器上，
  窗口尺寸按“点”算，drawable 是它的 2 倍像素。布局、鼠标坐标统一用窗口逻辑尺寸，
  `SDL_SetRenderLogicalPresentation(..., SDL_LOGICAL_PRESENTATION_STRETCH)` 负责把
  逻辑坐标映射到物理像素；`obtain_font()` 内部再按 `sdlgui_get_scale()` 放大字号，
  所以文字不会在 Retina 上变小或发虚。
- `SDL_GetWindowSize()` 返回逻辑尺寸，`SDL_GetWindowSizeInPixels()` 返回物理尺寸；
  hi-DPI 下两者不同，混用会让控件看起来缩水一半。

## 系统字体与嵌入字体

`assets/PacmanPixel-ASCII.ttf` 在构建时由 `tools/ttf_to_c.py` 转成
`build/pacman_pixel_font.c`，随后编译并自动链接进可执行文件，运行时不需要外部字体文件。
生成物只留在 `build/` 下，`make clean` 会一起清掉；修改字体后普通 `make` 会自动重新生成，
也可以显式运行 `make font`。

`sdlgui_init()` 先用内嵌字体预热第一个字号（20 pt），然后加载主题。
配置里的 `fonts` 数组是逻辑族名，`obtain_font()` 把“族名 + 字号”当缓存键：

- macOS：CoreText 找到字体文件；`.ttc` 集合会读 `name` 表匹配 PostScript 名，
  从而打开正确的 face（例如 `Helvetica Neue:style=Bold` 会命中 `HelveticaNeue-Bold`）。
- Linux：fontconfig 匹配，`FC_INDEX` 直接给出 face 序号。
- 找不到字体、或字体打不开时，回退到内嵌的像素字体。

具体用法和 SDL3_ttf 打开方式见 `tools/README.md`。

## 主题图标

图标规则和图标目录都由主题层管理：

- 配置根层可选 `icon_path`，相对 `config.toml` 所在目录；不写时默认
  `<配置目录>/icon`。
- `load_icon(path,name)` 只负责查找并加载：在 `path` 下找 `name.png`
  （`name` 已带 `.png` 时不重复添加），返回新纹理；它不修改规则。
- `theme_init()` 在 `sdlgui_init()` 中调用，负责预热默认字体，并从
  `icon_default_rules` 初始化 `icon_rules`。`icon_rules` 是按 name 升序排列的
  动态数组。
- `find_icon("name")` 找不到或名字非法时返回 `ICON_LOST`；`add_icon_rule(name,-1)`
  自动分配下标；`del_icon_rule(name,index)` 删除规则并回收不再使用的纹理。
- 规则名必须匹配 `[A-Za-z_][A-Za-z0-9_]*`：只允许大小写字母、数字和下划线，
  且数字不能作为首字母。非法名字会被 `add_icon_rule()` 拒绝并记录警告。
- `get_icon(index)` 在主题没有对应图标时回退到内置 lost 图标。

内置主题的 `icon_path` 为 `NULL`，它的 lost 图标是一个 64×64 的白色问号，
由 `tools/gen_lost_icon.py` 在构建时生成到 `build/lost_icon.c`，和嵌入字体一样
链接进可执行文件，运行时不需要外部图片。

同一张图也生成了一份 PNG 副本放在 `assets/Default/icon/lost.png`，所以默认主题
会直接读取这个文件。`get_icon()` 对所有 lost 图标（内置的、主题自带的）都会按
当前 `TEXT_CONTEXT` 的文字颜色着色，因此 Bright/Dark 两种配色下都能看清；
其他图标保持原色。外部主题缺少某个图标时，会优先回退到主题自己的
`icon/lost.png`，没有才使用内置问号。

需要重新生成这两份文件时：

```sh
python3 tools/gen_lost_icon.py -o build/lost_icon.c \
    --png assets/Default/icon/lost.png
```

### 图标 + 文字

`draft_create_icon_with_text(align, font, context, wl, hl, color)` 把图标和单行文字
合成到一张纹理里，`context` 支持内联图标：

- `\name`：取 `name` 的最长合法字符串（`[A-Za-z0-9_]*`）查图标并插入；
  找不到时回退到 lost 问号
- `\\`：输出一个反斜杠
- 末尾单独的 `\`：忽略
- 其他 `\x`：保留反斜杠，继续按普通文字处理

图标名按最长合法串解析：`\lostB` 会把 `lostB` 当成名字；要让 `B` 作为普通文字，
写成 `\lost B` 或 `\lost,B`。

文字不会自动换行；`wl` 大于 0 时按逻辑宽度裁剪。
`hl` 是图标高度（逻辑坐标）：`hl > 0` 时图标按这个高度渲染，宽度按原比例缩放；
`hl <= 0` 时退回字体行高。整体画布高度取字体行高和图标高度中的较大值，
图标和文字都垂直居中。
返回的 `draft_texture_t` 自己持有合成纹理，销毁 draft 时一并释放。

## 主题与资源搜索路径

`sdlgui_init(..., theme_name)` 查找 `<theme_name>/config.toml`；找不到指定主题时
再查 `Default/config.toml`，仍找不到则使用内置主题。`theme_name` 为
`NULL`/空串时等同于 `Default`。

未设置 `SDLGUI_THEME_PATH` 时，搜索路径依次是：

- 可执行文件/应用包所在目录下的 `assets`、`../assets`（开发时的仓库根）、`themes`
- macOS：`~/Library/Application Support/sdlgui`、`/Library/Application Support/sdlgui`、
  `/opt/homebrew/share/sdlgui`、`/usr/local/share/sdlgui`
- Linux：`/usr/share/sdlgui`、`~/.local/share/sdlgui`、`~/.config/sdlgui`

`SDLGUI_THEME_PATH` 环境变量会完全覆盖这份列表。例如在仓库根目录测试示例主题：

```bash
SDLGUI_THEME_PATH=assets ./build/pacman-gui
```

应用图标同理：`sdlgui_init()` 先按当前目录/绝对路径找，再找
`<可执行文件目录>/assets` 和 `<可执行文件目录>/../assets`。
macOS 的窗口图标 API 本身不生效，图标由应用包里的 `.icns` 提供。

## 本地化

日志和错误消息的 msgid 统一使用英语，通过 gettext 本地化，textdomain 为 `pacman-gui`。
运行时优先使用 `<可执行文件目录>/locale`，其次 `<可执行文件目录>/../locale`，
最后退回编译时的 `SDLGUI_LOCALEDIR`（默认当前目录的 `locale`）；也可以用
`-DSDLGUI_LOCALEDIR=\"...\"` 指定其他目录。`sdlgui_init()` 先把 locale 固定为 C，
内置主题保持英文；`theme_set()` 成功加载主题后才按环境 locale 调用 gettext。

macOS 默认没有 `libintl`。`src/sdlgui_i18n.h` 检测不到 `<libintl.h>` 时会把
`gettext` / `bindtextdomain` 换成空操作，界面回退英文；装了 Homebrew gettext 后
Makefile 会自动加 `-lintl` 和对应头文件路径。

## 加依赖：只需改 Makefile 一行

Linux（pkg-config 模块名）：

```make
PKGS := sdl3 sdl3-ttf fontconfig
```

macOS 不链 fontconfig，SDL3/SDL3_ttf 从 Homebrew 前缀取；要加 SDL3_image 之类的库，
在对应分支里加 `-lSDL3_image`（或把模块名加进 `PKGS`）即可。

GUI 里必然要显示文字，用 SDL3_ttf 渲染分数/包名列表的最小例子：

```c
#include <SDL3_ttf/SDL_ttf.h>

TTF_Init();
TTF_Font *font = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", 20);
SDL_Surface *surf = TTF_RenderText_Blended(font, "packages", 0, (SDL_Color){ 255, 255, 255, 255 });
SDL_Texture *tex  = SDL_CreateTextureFromSurface(renderer, surf);
SDL_FRect dst = { 20, 20, (float)surf->w, (float)surf->h };
SDL_RenderTexture(renderer, tex, NULL, &dst);
```

中文需要加载含中文字形的字体。macOS 可用
`/System/Library/Fonts/PingFang.ttc`，Linux 可用
`/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc`。

## 后续：把 pacman 换成 brew

目前的仓库还只是 GUI 骨架，没有接包管理后端。接后端时建议把平台差异收在独立模块：

- `src/backend.h`：定义查询/安装/删除/事务进度等回调；
- `src/backend_alpm.c`：Linux 用 libalpm（`pacman -S pacman` 提供头文件与库）；
- `src/backend_brew.c`：macOS 调用 `brew` 命令行（`brew info --json=v2`、
  `brew install` 等）或 Homebrew 的 API。

macOS 没有 libalpm，所以不要把 `alpm_*` 类型带进布局/绘制层；后台操作放到独立线程，
通过队列或 `SDL_Event` 回传给主循环，避免阻塞渲染。
