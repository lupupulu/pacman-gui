# pacman-gui

pacman-gui 是一个使用 C99 和 SDL3 编写的 Arch Linux 包管理器图形界面，当前版本 **v0.0.1**。

## 功能

- **包管理**
  - 通过 libalpm 读取本地已安装包，installed 页只显示用户显式安装的包。
  - 通过 `pacman -Ss` 搜索官方同步仓库；Homebrew 环境下也会搜索 formula/cask。
  - 通过 `pacman -Qu` 检查官方包更新，卡片版本显示为 `旧版本 -> 新版本`。
  - 对官方/非 AUR 包提供安装、更新、重装和删除；AUR/`pacman -Qm` 标记的外部包不显示操作按钮。
  - 包操作在后台线程执行，完成后重新枚举注册表并刷新 packages/installed 卡片。
  - 搜索、已安装列表和包操作的线程/注册表生命周期已做隔离。
- **界面**
  - SDL3 + SDL3_ttf，支持 Linux 与 macOS。
  - package / installed / settings 三个页签，顶部悬浮 selector，页面内容可上下滚动。
  - packages 和 installed 页各有防抖搜索框；列表分帧创建卡片，避免一次性卡住 UI。
  - 主题、图标、动画、圆角矩形、滑块、输入框和 selector 均可配置。
  - macOS 可打包成 `.app`。

详细实现说明见 [DEVELOPMENT.md](DEVELOPMENT.md)。

## 安装

### 依赖

Arch Linux：

```sh
sudo pacman -S --needed base-devel sdl3 sdl3_ttf fontconfig pacman pkgconf python
```

可选：`gettext`（本地化）、`git`（获取源码）。

macOS：

```sh
brew install sdl3 sdl3_ttf pkgconf python gettext
```

macOS 不需要 fontconfig；没有 `pkgconf` 时 Makefile 会回退到 Homebrew 前缀下的 include/lib。libalpm 后端只在能读取 `/var/lib/pacman/local` 时启用，Homebrew 后端只在找到 `brew` 时启用。

### 构建与运行

```sh
git clone https://github.com/lupupulu/pacman-gui.git
cd pacman-gui
make
./build/pacman-gui
```

常用命令：

```sh
make run                 # 编译并运行
make debug               # ASan/UBSan 调试版本
make OPT=-O3 LTO=1       # 优化 + LTO
make test-input          # 输入框测试程序
make app                 # macOS 应用包
make clean
```

安装/更新/删除官方包需要 root 权限；应用优先通过 `pkexec` 调用 polkit，没有 pkexec 时回退到 `sudo -n`。

更完整的编译选项、VS Code/clangd 配置和主题说明见 [DEVELOPMENT.md](DEVELOPMENT.md)。

## 许可

pacman-gui 使用 **GNU Lesser General Public License, version 3 or later**（`LGPL-3.0-or-later`）。

- 完整许可证文本：[LICENSE](LICENSE)
- `src/` 下原创源文件带有 `SPDX-License-Identifier: LGPL-3.0-or-later` 声明。
- `src/CDS/` 是第三方组件，按其自带的 [LICENSE](src/CDS/LICENSE) 授权，不适用本项目的 LGPL 声明。
- 除非另有说明，提交到本项目的贡献默认按 `LGPL-3.0-or-later` 授权。

## 贡献

1. Fork 本项目，从 `dev` 创建分支。
2. 遵循 [AGENTS.md](AGENTS.md) 中的 C 代码风格和布局/所有权约定。
3. 至少验证：

   ```sh
   make
   make test-input
   make debug          # 涉及内存/生命周期时
   git diff --check
   ```

4. 提交前说明改动目的、影响范围和验证方式，然后发起 Pull Request。

详细架构、控件 API、主题和后端扩展方式见 [DEVELOPMENT.md](DEVELOPMENT.md)。
