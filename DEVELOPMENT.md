# pacman-gui 开发文档

本文保留详细的控件、布局、主题和后端实现说明，以及开发/调试命令。
面向用户的安装、许可与贡献入口见 [README.md](README.md)。

## 依赖

- C99 编译器（gcc / clang）
- SDL3 + SDL3_ttf 开发包
- make
- Python 3.9+（构建时运行 `tools/ttf_to_c.py`，把字体嵌成 C）
- 字体解析：macOS 用系统 CoreText；Linux 用 fontconfig
- gettext（可选，仅用于本地化）
- libalpm（可选，Linux 上用于读取 pacman 已安装包；Arch 的 `pacman` 包提供）

Arch Linux：

```sh
sudo pacman -S sdl3 sdl3_ttf fontconfig pacman
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

# 优化级别 / 链接时优化；切换时 Makefile 会自动重建对象文件
make OPT=-O2 LTO=1
make OPT=-Os LTO=1
make OPT=-O3 LTO=1
```

`make` 在 macOS 上没有装 pkg-config 也能工作：Makefile 会退回
`$(brew --prefix)` 下的 `include/` 和 `lib/`；装了 pkg-config 则自动使用
`sdl3` 和 `sdl3-ttf`（或 `SDL3_ttf`）模块。Linux 上如果 `pkg-config` 能找到
`libalpm`，Makefile 会定义 `HAVE_LIBALPM`、编译 alpm 后端并链接 `-lalpm`；
找不到时不编译该后端，`pacman.c` 里对应的注册分支也不会生效。

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
src/init.c/.h  主界面布局、selector、tabs 和页面创建
src/pacman.c   包管理器后端注册表、统一包视图与软件包卡片
src/pacman.h    package_t、pacman_t 和后端接口
src/pacman_alpm.c/.h      libalpm 已安装包后端
src/pacman_homebrew.c/.h  Homebrew 后端（已安装枚举与可安装包搜索）
src/sdlgui.c   窗口、鼠标、布局、绘制与高 DPI 处理
src/sdlgui.h
src/sdlgui_theme.c   主题、配色、字体缓存与 TOML 配置
src/sdlgui_theme.h
src/sdlgui_fontlookup.c/.h   字体族名 -> 字体文件（macOS CoreText / Linux fontconfig）
src/sdlgui_widget.c/.h   按钮、开关、选择器与滑块控件
src/CDS/       单头文件配置/TOML 解析库
packaging/macos/Info.plist   macOS 应用包信息
build/pacman_pixel_font.c    构建时生成的嵌入字体源（已被 .gitignore 忽略）
build/lost_icon.c            构建时生成的内置 lost 图标源（已被 .gitignore 忽略）
assets/theme.toml            主题名映射（theme 参数 -> 主题目录名）
assets/Default/icon/*.svg     默认主题图标源文件
assets/Default/icon/lost.png  默认主题自带的 lost 图标（由 gen_lost_icon.py 生成）
assets/Default/icon/knob.png  默认主题自带的开关旋钮图标（运行时加载，不嵌入）
assets/Default/icon/package.png 安装包图标
assets/Default/icon/installed.png 已安装图标
assets/Default/icon/install.png 安装图标
assets/Default/icon/update.png 更新图标
assets/Default/icon/delete.png 删除图标
assets/Default/icon/settings.png 设置图标
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
- 主循环固定三段：`SDL_PollEvent` 收事件 → `sdlgui_handle_event()` 处理事件 →
  `SDL_RenderClear` / 绘制 / `SDL_RenderPresent`。`sdlgui_handle_event()` 封装了
  退出、Escape、窗口尺寸、系统主题、鼠标状态和输入框事件；返回 0 时退出主循环。
  `SDL_PollEvent` 本身仍留在 `main.c`。要按时间推进动画时，用
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
- `get_icon(index)` 对普通图标在主题缺失时回退到 lost；`ICON_KNOB` 例外，
  缺失时返回 `NULL`，开关控件只绘制轨道。

内置主题的 `icon_path` 为 `NULL`，它的 lost 图标是一个 64×64 的白色问号，
由 `tools/gen_lost_icon.py` 在构建时生成到 `build/lost_icon.c`，和嵌入字体一样
链接进可执行文件，运行时不需要外部图片。

开关旋钮图标是纯运行时主题资源，规则名是 `knob`，默认文件为
`assets/Default/icon/knob.png`；它不会嵌入可执行文件。主题没有提供旋钮图标时
开关只绘制轨道，不会回退到内嵌图片或色块。

同一张图也生成了一份 PNG 副本放在 `assets/Default/icon/lost.png`，所以默认主题
会直接读取这个文件。除 `ICON_KNOB` 外，`get_icon()` 会按当前 `TEXT_CONTEXT`
的文字颜色给图标着色，因此 Bright/Dark 两种配色下都能看清。外部主题缺少某个
图标时，会优先回退到主题自己的 `icon/lost.png`，没有才使用内置问号。默认主题
提供的图标规则包括 `lost`、`knob`、`package`、`installed`、`install`、
`update`、`delete` 和 `settings`。

默认主题图标的源文件是 `assets/Default/icon/*.svg`，运行时读取同名 64×64 PNG。
修改 SVG 后重新渲染：

```sh
sh tools/render_icons.sh
```

重新生成内置 lost 图标时：

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

## 开关控件

开关控件是独立的 `widget_switch_t`，不共用按钮结构，点击后在开/关之间切换：

```c
#include "sdlgui_widget.h"

static void on_switch(int on,void *data){
    // on: 0=关，1=开
}

widget_switch_t *sw = widget_create_switch(
    get_root_layout(),
    make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
    on_switch,
    NULL
);

widget_switch_set(sw,1);          // 程序化打开，不触发 callback
int on = sw->on;
```

点击回调的参数是切换后的状态（0/1），不是 `button_status_t`。

开关总长由主题根层的 `switch_width` 控制，高度沿用 `button_height`；
旋钮图标边长由 `switch_icon_height` 控制，`0` 表示跟随开关高度，默认与开关上下外沿对齐。
轨道颜色取主题 `color[]` 的开关部分：

- `COLOR_SWITCH_OFF`：关闭轨道
- `COLOR_SWITCH_ON`：打开轨道

开关悬停时不会额外高亮，只保留开/关两种轨道颜色。

旋钮使用主题图标 `ICON_KNOB`（默认文件 `assets/Default/icon/knob.png`），
按原图颜色绘制，不再按颜色字段着色。图标默认铺满开关高度，因此圆形边缘与
开关上下外沿对齐；主题没有提供该图标时 `widget_create_switch()` 返回 NULL。

旋钮位置由主题根层的 `switch_animation_time` 控制动画（默认 160 ms）。

不依赖主题的 raw 版本：

```c
widget_switch_t *sw = widget_create_raw_switch(
    parent,
    width,          // 轨道总长
    track_height,   // 轨道高度
    knob_height,    // 旋钮图标高度
    radius,         // 圆角半径；0=直角，<0 取轨道高度/2
    align,
    off_color,      // 关闭轨道颜色
    on_color,       // 打开轨道颜色
    animation_ms,   // 滑动动画时长
    knob_texture,   // 调用方提供的旋钮纹理，开关不负责释放
    on_switch,
    NULL
);
```

`widget_create_switch()` 只负责从主题取尺寸、颜色、动画时长和旋钮纹理，
然后调用 `widget_create_raw_switch()`。

## 设置行控件

`widget_item_t` 是设置页常见的“左侧说明 + 右侧控件”行：

```c
widget_item_t *s = create_item_switch(
    get_root_layout(), row_align,
    "\\settings Enable feature",
    on_switch, NULL
);

widget_item_t *b = create_item_with_button(
    get_root_layout(), row_align,
    "\\package Open settings",
    "\\settings Open",
    on_button, NULL
);

widget_item_t *sel = create_item_selector(
    get_root_layout(), row_align,
    "Theme",
    on_theme,
    NULL,
    "Light", "Dark", NULL
);
```

- 左侧文字使用 `draft_create_icon_with_text()`，因此支持 `\icon` 标记
- 右侧控件自动靠右、垂直居中
- 按钮宽度直接取主题根层 `switch_width`
- selector 每一项宽度取主题根层 `selector_item_width`
- 行高度取 `item_height`
- `create_raw_item()` 接受现成的文字 draft 和右侧控件，适合自定义行

配置示例：

```toml
switch_width = 32.0
selector_item_width = 72.0
```

`create_raw_item()` 成功后接管文字 draft；右侧控件会被挂到行布局下。

## 选择器控件

选择器是类似苹果顶栏/底栏的分段切换按钮：一条轨道上排列若干项，选中项用
滑块背景标出，点击后滑块平滑移动。轨道和选中滑块复用开关颜色：

```c
static void on_select(size_t now,size_t from,void *data){
    printf("select %zu (from %zu)\n",now,from);
}

widget_selector_t *selector = widget_create_selector(
    get_root_layout(),
    make_align(3,0,0,0,0.0f,40.0f,0,0),
    140.0f,                              // 每项宽度
    on_select,
    NULL,
    "\\lost Home",
    "\\knob Settings",
    "\\lost Profile",
    NULL                                 // 可变参数以 NULL 结尾
);
```

- 文字全部使用 `draft_create_icon_with_text()` 渲染，因此支持 `\icon` 标记。
- 轨道颜色为 `COLOR_SWITCH_OFF`，选中滑块为 `COLOR_SWITCH_ON`。
- 点击后 `callback(now,from,data)`，`now` 是当前项下标，`from` 是原下标。
- 滑动动画时长取主题根层 `switch_animation_time`。
- 高度取 `button_height`，并保证至少容纳一行字体；`controllable=0` 时忽略点击。
- `widget_create_selector_title()` 参数相同，但文字使用 `TEXT_TITLE`，高度取标题
  行高（至少 `button_height`），适合顶栏/底栏的大号选择器。
主题版本仍使用可变参数；不依赖主题的 raw 版本才使用数组 + 数量：

```c
widget_selector_t *selector = widget_create_raw_selector(
    parent,
    align,
    item_width,      // 每项宽度
    height,          // 选择器高度
    radius,          // 圆角半径；0=直角，<0 取高度/2
    font,
    items,           // const char *const *，可为 NULL
    count,
    track_color,     // 轨道颜色
    selected_color,  // 选中滑块颜色
    text_color,
    animation_ms,
    callback,
    data
);
```

setter：

```c
set_selector_item(selector, 1, "\\installed Installed");
set_selector_item_width(selector, 180.0f);
```

- `set_selector_item()` 会复制并替换第 i 项文字，重建该项文字纹理。
- `set_selector_item_width()` 会重设所有项目的宽度、对齐和文字换行宽度。
- `widget_create_selector()` / `_title()` 只负责从主题取字体、颜色和动画时长，
  然后调用 `widget_create_raw_selector()`。

## 滑块控件

滑块用于长列表的垂直滚动。滑块高度由“视口高度 / 内容总高度”比例决定，
并受 `slider_least_height` 限制；拖动时回调收到**绝对滚动偏移**，可直接交给
`list_set_slide()`：

```c
#include "sdlgui_widget.h"

static void on_slide(float offset,void *data){
    list_set_slide((layout_list_t *)data,offset);
}

layout_list_t *list = create_layout_list(...);
widget_slider_t *slider = create_layout_slider(
    get_root_layout(),
    make_align(1,3,0,1,-get_theme()->slider_width,0.0f,0.0f,1.0f), // 父布局右侧
    make_align(0,0,0,0,0,0,0,0),
    list_content_height,                  // 内容总长度
    0,                                    // 0=竖向，1=横向
    on_slide,
    list
);

// 滚轮/代码滚动列表后，把滑块位置同步过去
widget_slider_set(slider,list_get_slide(list));

// 内容总长度未知时可以直接查询 list
float content_height=list_get_content_height(list);
```

- `front_offset` / `back_offset` 是列表内容的前/后留白：滚动为 0 时第一个
  item 从 `front_offset` 之后开始，滚到末尾时最后一个 item 在 `back_offset`
  之前结束；`list_get_content_height()` 和滚动范围都包含这两段空间。
- 需要让内容从悬浮控件底下经过时，把列表铺满父布局，再用
  `front_offset` 给第一个 item 让位；父布局裁剪到整块区域，悬浮控件最后绘制。

- 滑块厚度取主题根层 `slider_width`；竖向时占父布局高度，横向时占父布局宽度。
- `all_height` 大于当前视口长度时才显示并响应鼠标；内容不足时自动隐藏且不触发回调。
- `lasting_time` 为鼠标停止操作后保持显示的毫秒数，`0` 表示始终显示；
  超过该时间后使用 `color_gradient()` 从滑块颜色渐变到全透明，时长由
  `slider_animation_time` 控制。
- 颜色取主题 `color[]`：
  - `COLOR_SLIDER`：普通
  - `COLOR_SLIDER_HIGHLIGHT`：拖动时

## 可滚动 Tab 布局

`create_layout_tab()` 创建一个整页内容可以上下滚动的页面布局（`layout_tab_t`）。
它和 `layout_list_t` 的区别是：list 负责一维排列子项；tab 的子布局是普通布局，
按 align 相对**整页内容**（宽度等于页面宽度，高度等于整页高度）定位，适合设置页、
详情页这类带任意子控件的可滚动页面。

```c
layout_tab_t *page = create_layout_tab(
    parent,
    0.0f,
    0.0f,
    make_align(3,3,1,1,0.0f,0.0f,1.0f,1.0f),
    1200.0f,          // 整页高度；<=0 表示按子布局自动计算
    header,           // 子布局的 align 相对整页计算
    form,
    footer,
    NULL
);
page->front_offset = 24.0f;
page->back_offset = 24.0f;
```

- `l` 是可视区域；`to_draw` 是整页内容布局，子布局挂在 `to_draw` 下。
- `content_height` 固定整页高度；传 0/负数时自动取
  `max(视口高度, 子布局下边界)`。
- `align` 为 0 的子布局按整页相对坐标处理，会随内容一起滚动。
- `tab_add()` 追加子布局；`tab_slide()` / `tab_set_slide()` / `tab_get_slide()`
  负责滚动；`tab_get_content_height()` 返回含前后留白的总长度，可直接交给 slider。
- 视口裁剪由 `l` 负责；完全在视口外的子布局不参与鼠标命中，滚轮优先由
  子布局消费，没人消费时 tab 自己滚动。
- 自动高度适合按 y 堆叠的固定高度子布局；如果子布局的 height / vertical align
  依赖整页高度，请使用固定 `content_height`，避免高度和子布局互相影响。

## Label 布局

`create_layout_label()` 在 `sdlgui.c` 里提供了一个简单的文字标签布局：

```c
TTF_Font *font = obtain_font(get_font(TEXT_CONTEXT),
                             get_text_size(TEXT_CONTEXT));

layout_label_t *label = create_layout_label(
    parent,
    300.0f, 80.0f,                         // 标签宽高
    make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f), // 标签自身位置
    make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f), // 文字在标签内居中
    font,
    "标签文字",
    get_text_color(TEXT_CONTEXT)
);
```

- `text_align` 的 `horizon`：`0=左`、`1=右`、`3=居中`
- `text_align` 的 `vertical`：`0=上`、`1=下`、`3=居中`
- 文字会按标签宽度自动换行，并在标签高度处裁剪
- 传入的文字会被复制，调用方可以立即释放临时字符串
- 标签自身通过 `align` 参与布局更新，`text_align` 控制文字在标签内的对齐

## Package 卡片

`create_layout_package(package_t *package)` 创建一个高 104 的软件包卡片。
卡片实现放在 `pacman.c` 里（原来的 `pacman_gui.c/.h` 已合并进去），
`pacman.h` 只暴露这一个构造函数：

- 包名：`TEXT_SUBTITLE`
- 版本、维护者/提交者、许可证：`TEXT_CONTEXT`
- 右侧按钮上下排列且等宽：
  - 未安装：`\\install install`
  - 已安装且检测到新版本：上方 `\\update update`，下方 `\\delete delete`
  - 已安装且已是最新：上方 `\\install reinstall`，下方 `\\delete delete`
- 版本行在可更新时显示 `Version: 旧版本 -> 新版本`
- maintainer 和 submitter 都为空时，整行人员信息不创建、不显示
- `licenses` 为空时不创建许可证行
- 只有 `package->from->official` 为 1 且 `package->foreign == 0`
  （官方同步仓库、非 AUR/手动安装）的卡片才创建按钮；AUR/外部包或没有
  包管理能力的后端只显示信息。按钮通过 `pacman_action_start()` 把操作交给
  后台线程，不会在回调里直接跑 pacman。

`init.c` 里创建 tabs 和页面布局，`main.c` 只负责初始化和事件循环：
- `package` 页：搜索可安装包，结果卡片带 install 按钮
- `installed` 页：所有已注册后端的已安装包列表，可滚动，右侧带滑块
- `settings` 页：空矩形

installed 页是一个容器，里面放 `layout_list_t`、右侧 `widget_slider_t`，
顶部还有一行搜索框（`widget_create_search_input()`）和空结果提示；
滑块通过 `list_get_content_height()` 获取内容总长度，并在页面更新时同步
`list_get_slide()`。卡片宽度由所在的 `layout_list_t` 决定，内部标签和按钮会在
`package_update()` 中按卡片宽度重新定位。后端没有提供 maintainer/submitter 时，
人员信息行会被省略。UI 初始化时列表为空，后台枚举线程结束后由主循环
推进“已枚举”游标：`init_ui_begin_refresh_packages()` 记录包总数，
`init_ui_refresh_packages_batch(32)` 每帧推进游标，卡片则由
`installed_page_update()` 按 `PAGE_CARDS_PER_FRAME` 每帧补进列表，
因此建卡和输入搜索词期间界面都保持可响应。

搜索框的 `change_callback` 只记录过滤词和脏标记，清空列表并重新扫描放在
`installed_page_update()` 里做，避免在遍历子布局的过程中删除列表项。
过滤不区分 ASCII 大小写，只匹配包名；没有命中时显示 “No matching packages”。

### packages 搜索

`package` 页走的是后端搜索，不依赖本地已安装列表：

- 输入停顿 `PACKAGES_SEARCH_DELAY`（350 ms）后自动搜索，回车立即搜索
- 搜索在 `pthread` 里调用 `pacman_search_all(keyword)`，结果写进独立的
  可安装包注册表 `pkg_available`；主线程 `pthread_join` 之后才读取，
  和已安装列表的线程约定一致
- `join` 后用 `pacman_available_count()/pacman_available_at()` 按
  `PAGE_CARDS_PER_FRAME` 每帧补卡片，所以结果很多时界面也不卡
- 结果里已经安装过的包会被标记为 `PKG_STAT_INSTALLED` 并补上本地版本号，
  有更新时同步 `update_available/new_version`，卡片显示 update/delete 或
  reinstall/delete 按钮
- 显式已安装列表不包含作为依赖安装的包；搜索结果会再通过后端的
  `query_installed()` 查本地数据库，所以 `gnu-free-fonts` 这类依赖包也会正确
  显示为 reinstall/delete，而不会显示成可以 install
- 已安装列表装载完成前（`pacman_installed_ready()` 为 0）不会读取
  `pkg_installed`；loader 完成后主循环会再刷新一次 packages 页，补上安装状态
- 关键词为空时只清空结果，不启动线程；没有结果时显示 “No matching packages”，
  还没搜索时显示输入提示

页面与 tabs 一样大，selector 浮在页面之上。`widget_create_tabs()` 会把传入的
selector 标记为 `draw_last`，保证它晚于页面绘制。两个包页面都把列表的
`front_offset` 设为 selector 高度加搜索框高度，`back_offset` 留出 selector
高度：第一个 item 从搜索框下缘开始，滚动时可以从 selector 和搜索框底下经过；
末尾也留出同样的空间。

```c
page_apply_header(&installed_page->rect,&installed_page->header_offset,
    installed_page->search,installed_page->list,selector->l.l.height);
```

## 包管理器后端

`pacman.c` 维护已注册后端列表，以及一个所有后端共用的全局已安装包数组
`pkg_installed`。installed 页不直接引用具体后端，而是通过 `pacman.h` 的
统一接口遍历：

```c
size_t count=pacman_package_count();
for(size_t i=0;i<count;i++){
    package_t *pkg=pacman_package_at(i);
}
```

`pkg_installed` 以 `package_t.name` 升序排列，增删由 `pacman.c` 统一负责：

- `package_t.from` 现在是指向所属后端的 `pacman_t *`；它不再是字符串，
  AUR 后端将来只需要提供自己的 `pacman_t`，注册表就能区分来源。
- `pkg_add()` 把包插入有序位置；如果 `from/name/meta_data/version` 全部相同
  的包已经存在，说明该后端的缓存过期，会先 `pkg_del_from(pkg->from)` 再调用
  `pkg->from->list_installed()` 重建。
- `pkg_del()` 按这四项精确匹配并删除；找不到时同样触发对应后端的重建。
- `pkg_del_from(pacman_t *from)` 删除来自给定后端的全部包。
- `pkg_add()` 接管 `name/version/new_version/description/licenses/submitter`
  的所有权；`from` 是后端指针，`meta_data` 是后端借用的常量字符串，
  注册表不释放它们。

`pacman_backend_find("alpm")` 按 `pacman_t.name` 查找已注册后端。

### 可安装包视图

`package` 页用另一套注册表 `pkg_available`，和已安装列表完全分开：

- 后端实现 `pacman_t.search(keyword)`，通过 `pkg_available_add()` 把搜索结果
  放进来；`from` 是后端指针，`meta_data` 是后端借用的字符串，其余字段由
  注册表接管；
- `pkg_available_add()` 按 `from/name/meta_data` 去重，和 `pkg_add()` 不同，
  它不会触发 `pkg_resync()`，避免在搜索线程里回头写已安装数组；
- 新条目追加到末尾，**保持后端的返回顺序、不按字母排序**：搜索结果的相关性
  由后端决定，页面也只按数组顺序生成卡片；
- `pacman_search_all(keyword)` 先清空注册表，再依次调用所有支持搜索的后端；
- 主线程用 `pacman_available_count()` / `pacman_available_at()` 按数组顺序遍历；
- `pacman_installed_find(package->from,name)` 用来判断搜索结果是否已安装；
- `pacman_quit_all()` 会一并释放可安装注册表。

搜索线程只写 `pkg_available`，主线程 `pthread_join` 之后才读；页面在等待结果
期间把 `scan_running` 置 0，因此不会并发访问。

默认注册逻辑在 `register_default_backends()` 中：

- `pacman_alpm_available()` 为真时注册 libalpm 后端；
- `pacman_homebrew_available()` 为真时注册 Homebrew 后端；
- 两个条件都不满足时后端列表为空，installed 页保持空列表，package 页搜不到结果。

因此没有安装 Homebrew 的系统不会注册 Homebrew 后端，也不会执行 `brew` 命令。

### libalpm 后端

`pacman_alpm.c` 用于读取本机 pacman 本地数据库：

- 只有编译时检测到 `libalpm`，并且 `/var/lib/pacman/local` 可读时才启用；
- `init()` 用 `alpm_initialize("/","/var/lib/pacman/",&err)` 打开数据库；
- `list_installed()` 通过 `alpm_get_localdb()` / `alpm_db_get_pkgcache()`
  遍历已安装包，并用 `pkg_add()` 加入全局数组；
- 只保留 `alpm_pkg_get_reason()` 为 `ALPM_PKG_REASON_EXPLICIT` 的包，
  作为依赖自动安装的包不显示；
- 填入名称、版本、描述、许可证、packager、安装大小；
- `stat = PKG_STAT_INSTALLED`，`from` 指向 `pacman_alpm()` 返回的后端。

### 官方仓库搜索、更新检查与包操作

`pacman_alpm.c` 的本地数据库仍用 libalpm，同步数据库查询和需要 root 的
安装事务交给 `pacman` 命令本身，避免在 GUI 里重复解析 `pacman.conf`：

- `search(keyword)` 执行
  `pacman -Ss --color never -- <keyword>`，只解析官方同步仓库的结果；
  每行的 `repo/name version` 变成 `package_t`，`from` 指向 alpm 后端，
  `meta_data` 为 NULL，`stat = PKG_STAT_NULL`。
- `list_installed()` 结束后执行 `pacman -Qm --color never`，把不在官方同步
  仓库里的 AUR/手动安装包标记为 `foreign = 1`；`pacman -Qm` 失败时保守地
  把所有 alpm 包标成 foreign，避免误操作。
- `check_updates()` 执行 `pacman -Qu --color never`，把可升级包的
  `new_version` 和新版本标记写入已安装注册表；foreign 包不参与更新检查。
  安装列表加载后会自动调用一次，所以官方包卡片能显示 `旧版本 -> 新版本`。
- `install_package()` / `update_package()` / `uninstall_package()` 分别执行
  `pacman -S --asexplicit`、`pacman -S --asexplicit`、`pacman -R`
  （均带 `--noconfirm`）。显式安装不使用 `--needed`：如果目标已经作为依赖
  安装，`pacman -S` 会重装，而 `--asexplicit` 保证安装原因变为 explicit，
  操作完成后能同步进入 installed 页；`gnu-free-fonts` 这类依赖包因此不会再
  出现“点击安装后 installed 里没有”的情况。
  普通用户下优先用 `pkexec` 触发 polkit；没有 pkexec 时退回
  `sudo -n`，没有提权工具就报错。命令通过 argv 传给 `execvp`，不经过 shell。
  动作不捕获 pacman 的 stdout/stderr，只看退出码：pacman hook 可能 fork 出
  继承管道的后台进程，如果等待 pipe EOF，包已经删掉但后台线程会一直不结束，
  主循环就不会重新枚举注册表。
- 只有 `official = 1` 且包本身不是 foreign 的后端/包允许这些操作；当前只有
  alpm 设置 official，Homebrew 和未来的 AUR 后端不会走 pacman 事务。
- 所有命令都在后台线程执行。`pacman_action_start()` 会复制包名等字段，
  不继续持有注册表里的 `package_t`；主循环用 `pacman_action_poll()` 等待
  完成，然后清空旧卡片、重新枚举并重建 UI。
- 测试时可以设置 `PACMAN_GUI_DRY_RUN=1` 只记录命令不执行；
  `PACMAN_GUI_PACMAN=/path/to/fake-pacman` 可以替换 pacman 命令做解析测试。

Makefile 通过 `pkg-config --exists libalpm` 做编译期判断：找到时定义
`HAVE_LIBALPM=1`、编译 `pacman_alpm.c` 并链接 `-lalpm`；没找到时不把该源文件
加入 `SRCS`。`pacman.c` 用 `#ifdef HAVE_LIBALPM` 决定是否包含头文件并注册后端。

### Homebrew 后端

Makefile 在构建时检查 `PATH`、`HOMEBREW_PREFIX`、`/opt/homebrew`、
`/usr/local` 和 `/home/linuxbrew` 下有没有 `brew`：找到就定义
`HAVE_HOMEBREW=1` 并编译 `pacman_homebrew.c`，否则不把该源文件加入 `SRCS`。
交叉编译或打包机没装 brew 时，可以用 `make HAVE_HOMEBREW=1` 强制编入；
想明确排除时用 `make HAVE_HOMEBREW=0`。

`pacman_homebrew.c` 实现了 `pacman_t` 的 Homebrew 版本。Homebrew 的
`official` 为 0，因此不显示 pacman 的安装/更新/删除按钮：

- `pacman_homebrew_available()` 依次检查 `HOMEBREW_PREFIX/bin/brew`、
  `/opt/homebrew/bin/brew`、`/usr/local/bin/brew`、
  `/home/linuxbrew/.linuxbrew/bin/brew` 和 `PATH`；
- 找到 brew 才会注册后端，`init()` 只检查 brew 是否可用，
  `list_installed()` 调用 `brew list --versions --formula` 和
  `brew list --versions --cask`，再通过 `pkg_add()` 加入全局数组；
- 每个包创建一条 `package_t`，`stat` 统一设为 `PKG_STAT_INSTALLED`，
  `from` 指向 Homebrew 后端，`meta_data` 标记 `formula` 或 `cask`；
- Homebrew CLI 文本没有 licenses、maintainer、submitter、描述等字段，这些内容
  使用卡片的空值回退；
- `search(keyword)` 调用 `brew search --formula <keyword>` 和
  `brew search --cask <keyword>`（带 `HOMEBREW_NO_AUTO_UPDATE=1`，避免搜索时
  触发自动更新），逐行取包名并通过 `pkg_available_add()` 加入可安装视图；
- 关键词只保留 `[A-Za-z0-9@._+-/]`，确保不会被拼进 shell 命令；
- 搜索结果没有版本号，先填 `unknown`；命中已安装列表时由页面补上本地版本号。

`pacman_init_all()` 只初始化各后端，不再枚举软件包。`main()` 在
`sdlgui_init()` 之前调用它；`init_ui()` 完成后，主线程创建一个 pthread 执行
`pacman_list_installed_all()`，该线程通过各后端的 `list_installed()` /
`pkg_add()` 填充全局数组。主循环用 `SDL_AtomicInt` 轮询完成标志，
`pthread_join` 之后调用 `init_ui_begin_refresh_packages()`；之后每帧调用
`init_ui_refresh_packages_batch(32)` 追加一批卡片，建卡期间仍回到事件循环，
因此窗口可以先显示，枚举和建卡都不会长时间阻塞交互。

枚举期间 UI 不读取 `pkg_installed`；退出时主线程也会先 join 枚举线程，
再调用 `pacman_quit_all()` 释放后端和包数据。

## Tabs 控件

Tabs 只负责页面切换，不再创建 selector。调用方先创建 selector，再传给 tabs：

```c
static void on_tab(size_t now,size_t from,void *data){
    // now: 新页下标，from: 旧页下标
}

static void tabs_size(float *w,float *h){
    *w = get_root_w();
    *h = get_root_h();
}

widget_selector_t *selector = widget_create_selector_title(
    get_root_layout(), selector_align, 200.0f,
    on_tab, NULL,                       // 用户 callback 留在 selector 上
    "\\package 软件包",
    "\\installed 已安装",
    "\\settings 设置",
    NULL
);

widget_tabs_t *tabs = widget_create_tabs(
    get_root_layout(),
    make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f),
    tabs_size,
    selector
);

layout_t *pages[] = { package_page, installed_page, settings_page };
widget_set_tabs(tabs, pages, 3);
```

- selector 的 parent 为 `NULL` 时，tabs 才把它挂到自己下面并管理宽度
- selector 已经挂在其他父布局下时，tabs 保持它的位置和宽度不变；页面与 tabs
  等大，tabs 会把 selector 标记为 `draw_last`，让它绘制在页面之上
- tabs 会保存 selector 原来的 callback/data，覆盖为内部转发函数：
  先切换页面，再调用原 callback
- 删除 tabs 时，外部 selector 的 callback/data 会恢复
- tabs 不再接收自己的 callback 参数
- `get_size` 每次 update 都会被调用，返回 tabs 的总宽高；返回 0 的字段沿用布局尺寸
- 只有当前页 `enable/active=1`，其余页不更新、不绘制、不接收鼠标
- 页面布局由 tabs 接管，删除 tabs 时一并释放
- `widget_tabs_select(tabs, index)` 可程序化切换；有 selector 时同步其选中状态
- `widget_set_tab(tabs, page, index)` 可单独设置/替换某一页

## 输入框布局

`create_layout_input()` 负责输入框的交互和文字显示：

- 鼠标进入布局时切换为 I-beam 文本光标
- 鼠标离开时恢复默认光标
- 左键点击布局后获取焦点，开始接收键盘输入
- 点击输入框外部时自动失去焦点
- 维护 UTF-8 文本、光标位置、最大字节数和焦点状态
- 内部用文字 draft 绘制输入内容，缓冲区变化时立即重建纹理
- 聚焦和光标/滚动变化时调用 `SDL_SetTextInputArea()`，把 IME 候选窗锚定到
  当前光标的小矩形，而不是窗口角落
- 背景和文字光标仍由调用方绘制

```c
static void on_submit(const char *text,void *data){
    printf("submit: %s\n",text);
}

TTF_Font *font = obtain_font(get_font(TEXT_CONTEXT),
                             get_text_size(TEXT_CONTEXT));

layout_input_t *input = create_layout_input(
    get_root_layout(),
    320.0f,                     // 宽
    44.0f,                      // 高
    make_align(3,0,0,0,0.0f,120.0f,0,0),
    "",                         // 初始文字
    font,
    get_text_color(TEXT_CONTEXT),
    128,                        // 最大字节数，0=不限制
    on_submit,                  // 回车回调，可为 NULL
    NULL
);
```

主循环里把事件交给统一处理函数：

```c
while(SDL_PollEvent(&ev)){
    if(!sdlgui_handle_event(&ev)){
        break;                  // QUIT / 无输入焦点时的 Esc
    }
}
```

`sdlgui_handle_event()` 会优先把文本和编辑键交给当前焦点输入框，然后处理
窗口、主题、鼠标等系统事件。`SDL_PollEvent` 保留在调用方主循环中。

支持的编辑操作：

- 普通文字输入
- `Backspace` / `Delete`
- `Left` / `Right`
- `Home` / `End`
- `Enter` 调用回调
- `Esc` 失去焦点
- `Ctrl+V` / `Cmd+V` 粘贴

滚动：

- 鼠标在输入框内、且文字宽度超过输入框宽度时，消费滚轮的左右横移
- 横移会调整 `input->text_scroll`，文字渲染起点随之左右滑动
- 文字较短时不消费横向滚轮
- 文字变短或输入框变宽后，偏移会自动夹回有效范围

调用方读取状态绘制背景/光标：

```c
const char *text = layout_input_text(input);
size_t cursor = layout_input_cursor(input);
int focused = layout_input_is_focused(input);

// 文字 draft 的横向起点已经按 text_scroll 调整
float scroll = input->text_scroll;
float text_width = input->text_width;
```

`cursor` 是 UTF-8 字节下标，并且始终落在字符边界上。`max_bytes` 限制的是
文本字节数（不含结尾 `\0`），截断时不会留下半个 UTF-8 字符。
背景、光标和光标闪烁由外部根据这些状态绘制；需要现成的外观和鼠标交互时
用下面的 `widget_create_input()`。

`layout_input` 也提供程序化编辑接口，供控件层组合使用：
`layout_input_set_text()`、`layout_input_set_cursor()`、
`layout_input_set_cursor_at()` 和 `layout_input_scroll_into_view()`。

输入框使用的字体必须包含要显示的文字字形。例如主题默认的 `monospace`
在 macOS 上通常是 Menlo，不包含中文字形；需要中文时请传入
`PingFang SC` 等包含 CJK 字形的字体。

## 输入框控件

`widget_create_input()` 在 `layout_input_t` 之上补全外观和鼠标交互，
开箱即用；`widget_create_search_input()` 是搜索框版本，额外带放大镜图标和
清除按钮。

```c
static void on_search_change(const char *text, void *data){
    // 每次内容变化后调用：键盘、粘贴、清除按钮、set_text 都会触发
}

widget_input_t *search = widget_create_search_input(
    panel,
    0.0f,                       // 宽度，<=0 时取父布局宽度
    make_align(3,0,2,0,0.0f,52.0f,-32.0f,0.0f),
    "搜索已安装的软件包",
    on_search_change, NULL,     // 内容变化回调
    NULL, NULL                  // 回车提交回调
);
```

外观：

- 圆角背景 + 1 像素边框，圆角取自主题的 `rect_round_corner`
- 悬停、聚焦时背景色和边框色平滑渐变到对应颜色，时长取
  `button_animation_time`
- 没有内容时显示占位文字；占位文字有自己的子布局，裁剪范围等于文字区
- 聚焦后显示文字光标，编辑后先常亮一个周期再按半周期闪烁
- 左侧图标和右侧清除按钮可选；图标与 × 都在创建时按物理像素程序化生成，
  不依赖字体字形或主题图片资源，绘制时按 `icon_color` 染色

交互：

- 点击文字区后聚焦，并把光标放到离鼠标最近的字符边界；按住拖动继续移动
- 点击清除按钮清空内容并保持焦点
- 内容为空时不显示清除按钮；`controllable=0` 时忽略鼠标但键盘仍可用
- 回车调用 `submit_callback`，`Esc` 失焦（由 `layout_input` 处理）
- 文字溢出时自动滚动，让光标保持可见；横向滚轮仍可手动滚动

```c
widget_input_set_text(search, "font");        // 改写内容，光标移到末尾
widget_input_set_placeholder(search, "搜索");  // 替换占位文字
widget_input_set_icon(search, get_icon(...)); // 借用的纹理，控件不释放
widget_input_set_clear(search, 1);            // 启用清除按钮
widget_input_set_max_bytes(search, 96);       // 0 表示不限制
widget_input_focus(search);                   // 直接聚焦
```

`change_callback` 在控件自己的 update 里检测文本变化后调用，所以
`sdlgui_handle_event()` 处理完键盘事件后，下一帧就能收到回调；
`widget_input_set_text()` 同样通过这个路径通知调用方。

输入框控件不读取主题时用 `widget_create_raw_input()`：尺寸、圆角、字体、
背景/悬停/聚焦色、边框色、文字/占位/光标/清除按钮颜色和动画时长全部由
调用方传入，适合嵌到自定义配色的面板里。

## 输入框测试程序

仓库里有一个可交互的输入框测试程序：

- 源文件：`tests/input_test.c`
- 构建并运行：`make test-input`
- 测试布局只绘制文字，背景、边框和文字光标由测试程序绘制
- 初始文本包含中文，用于验证中文渲染
- 自动优先使用 `PingFang SC` 等 CJK 字体，找不到时回退主题字体
- 初始文本较长，用于验证横向滚轮滑动
- 最大字节数为 96

运行后可以：

- 点击输入框聚焦并输入，可使用系统输入法
- 输入法候选窗会锚定在文字光标附近
- 文字溢出时用横向滚轮滑动
- `Enter` 在日志中打印提交内容
- `Esc` 先失焦，再按一次退出

## 主题与资源搜索路径

`sdlgui_init(..., theme_name)` 先读取主题搜索路径中的 `theme.toml`：

- `theme_name` 非空时，用 `theme_name` 查同名键；
- `theme_name` 为 `NULL`/空串时，用 `Default` 查键；
- 命中时，用键对应的值作为实际主题名；
- 没有 `theme.toml`、没有对应键或值无效时，直接使用 `theme_name`（NULL/空串时为 `Default`）。

之后按实际主题名查找 `<主题名>/config.toml`；找不到时再查
`Default/config.toml`，仍找不到则使用内置主题。主题加载逻辑独立在
`sdlgui_theme_init()` 中，由 `sdlgui_init()` 调用。

例如仓库里的 `assets/theme.toml`：

```toml
Default = "Default"
```

也可以把某个别名映射到其他主题目录，例如 `Foo = "Default"`。

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

## 后端扩展

平台/包管理器差异都收在独立后端模块里，布局和绘制层不直接引用 `alpm_*` 或
`brew` 命令：

- `src/pacman_alpm.c`：Linux 用 libalpm 读取本地数据库；
- `src/pacman_homebrew.c`：调用 `brew` 命令行读取 formula/cask。

后续接入安装、删除、更新或后台事务时，建议放到独立线程，通过队列或
`SDL_Event` 回传给主循环，避免阻塞渲染。
