# ============================================================
#  SDL3 项目模板 Makefile
#    make        编译到 build/pacman-gui
#    make run    编译并运行
#    make debug  带 ASan/UBSan 与调试信息编译
#    make app    （macOS）生成 build/pacman-gui.app
#    make compdb 生成 compile_commands.json（VS Code / clangd 用）
#    make clean  清理构建产物
#    make font   重新生成 build/pacman_pixel_font.c
# ============================================================

TARGET  := build/pacman-gui
SRCDIR  := src
# src/*.c 加上一级子目录（src/CDS/ 里的单头文件库实现）
SRCS    := $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*/*.c)

# 嵌入字体：构建时把 assets/ 里的 TTF 转成 build/pacman_pixel_font.c，
# 再和 src/*.c 一起编译链接。生成物全部留在 build/ 下，make clean 会清掉。
FONT_TTF := assets/PacmanPixel-ASCII.ttf
FONT_C   := build/pacman_pixel_font.c
FONT_SYM := pacman_pixel_ttf
FONT_OBJ := build/obj/pacman_pixel_font.o

# 内置图标：构建时分别由 tools/gen_lost_icon.py、tools/gen_switch_icon.py 生成
LOST_ICON_C   := build/lost_icon.c
LOST_ICON_OBJ := build/obj/lost_icon.o
SWITCH_ICON_C   := build/switch_icon.c
SWITCH_ICON_OBJ := build/obj/switch_icon.o

OBJS    := $(patsubst $(SRCDIR)/%.c,build/obj/%.o,$(SRCS)) $(FONT_OBJ) $(LOST_ICON_OBJ) $(SWITCH_ICON_OBJ)
DEPS    := $(OBJS:.o=.d)

CC       ?= cc
PKGCONF  ?= pkg-config
PYTHON   ?= python3

UNAME_S := $(shell uname -s)
HAVE_PKGCONFIG := $(shell command -v $(PKGCONF) >/dev/null 2>&1 && echo 1)

# Homebrew 前缀：PATH 里没有 brew 时退回常见安装位置
BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null || (test -d /opt/homebrew && echo /opt/homebrew || echo /usr/local))

CFLAGS  += -std=c99 -Wall -Wextra -Wshadow -O2 -g -MMD -MP
LDFLAGS +=
LDLIBS  +=

ifeq ($(UNAME_S),Darwin)
  # macOS 用系统 CoreText 解析字体，不依赖 fontconfig
  CFLAGS += -DSDLGUI_USE_CORETEXT=1
  # src/CDS 里的单头文件库为跨编译器重复 typedef 了 size_t（C11 才正式允许）
  CFLAGS += -Wno-typedef-redefinition
  LDLIBS += -framework CoreFoundation -framework CoreText

  ifneq ($(HAVE_PKGCONFIG),)
    TTF_PKG := $(shell $(PKGCONF) --exists sdl3-ttf && echo sdl3-ttf || echo SDL3_ttf)
    PKGS    := sdl3 $(TTF_PKG)
    CFLAGS  += $(shell $(PKGCONF) --cflags $(PKGS))
    LDLIBS  += $(shell $(PKGCONF) --libs $(PKGS))
  else
    # 没装 pkg-config 时直接使用 Homebrew 前缀
    CFLAGS += -I$(BREW_PREFIX)/include
    LDLIBS += -L$(BREW_PREFIX)/lib -Wl,-rpath,$(BREW_PREFIX)/lib -lSDL3 -lSDL3_ttf
  endif

  # gettext 可选：装了就有真正的本地化，没装则界面回退英文
  ifneq ($(wildcard $(BREW_PREFIX)/include/libintl.h),)
    CFLAGS += -I$(BREW_PREFIX)/include -DSDLGUI_HAVE_GETTEXT=1
    LDLIBS += -L$(BREW_PREFIX)/lib -lintl
  endif
else
  TTF_PKG := $(shell $(PKGCONF) --exists sdl3-ttf && echo sdl3-ttf || echo SDL3_ttf)
  PKGS    := sdl3 $(TTF_PKG) fontconfig
  CFLAGS  += $(shell $(PKGCONF) --cflags $(PKGS))
  LDLIBS  += $(shell $(PKGCONF) --libs $(PKGS))
endif

# compile_commands.json 片段：clang -MJ 每次编译写一条，最后合并
COMPDB_DIR := build/compdb
ifeq ($(COMPDB),1)
COMPDB_FLAG      = -MJ $(COMPDB_DIR)/$(subst /,_,$*).json
COMPDB_FONT_FLAG = -MJ $(COMPDB_DIR)/pacman_pixel_font.json
COMPDB_ICON_FLAG        = -MJ $(COMPDB_DIR)/lost_icon.json
COMPDB_SWITCH_ICON_FLAG = -MJ $(COMPDB_DIR)/switch_icon.json
else
COMPDB_FLAG      =
COMPDB_FONT_FLAG =
COMPDB_ICON_FLAG =
COMPDB_SWITCH_ICON_FLAG =
endif

# 内存/未定义行为检查版本（注意会明显变慢）
ifeq ($(BUILD),debug)
CFLAGS  += -O0 -g3 -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif

.PHONY: all run debug clean font app run-app compdb

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

build/obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@) $(COMPDB_DIR)
	$(CC) $(CFLAGS) $(COMPDB_FLAG) -c $< -o $@

$(FONT_C): $(FONT_TTF) tools/ttf_to_c.py
	@mkdir -p $(dir $(FONT_C))
	$(PYTHON) tools/ttf_to_c.py $(FONT_TTF) -o $(FONT_C) --symbol $(FONT_SYM)

$(FONT_OBJ): $(FONT_C)
	@mkdir -p $(dir $@) $(COMPDB_DIR)
	$(CC) $(CFLAGS) $(COMPDB_FONT_FLAG) -c $(FONT_C) -o $@

$(LOST_ICON_C): tools/gen_lost_icon.py
	@mkdir -p $(dir $(LOST_ICON_C))
	$(PYTHON) tools/gen_lost_icon.py -o $(LOST_ICON_C)

$(LOST_ICON_OBJ): $(LOST_ICON_C)
	@mkdir -p $(dir $@) $(COMPDB_DIR)
	$(CC) $(CFLAGS) $(COMPDB_ICON_FLAG) -c $(LOST_ICON_C) -o $@

$(SWITCH_ICON_C): tools/gen_switch_icon.py
	@mkdir -p $(dir $(SWITCH_ICON_C))
	$(PYTHON) tools/gen_switch_icon.py -o $(SWITCH_ICON_C)

$(SWITCH_ICON_OBJ): $(SWITCH_ICON_C)
	@mkdir -p $(dir $@) $(COMPDB_DIR)
	$(CC) $(CFLAGS) $(COMPDB_SWITCH_ICON_FLAG) -c $(SWITCH_ICON_C) -o $@

run: $(TARGET)
	./$(TARGET)

debug:
	$(MAKE) clean
	$(MAKE) BUILD=debug

# 用 clang -MJ 生成编译数据库；VS Code 的 C/C++ 扩展和 clangd 都会读取
compdb:
	@if ! $(CC) --version 2>/dev/null | grep -qi clang; then \
		echo "make compdb: 需要 clang（Linux 可改用 bear -- make）" >&2; \
		exit 1; \
	fi
	$(MAKE) clean
	@mkdir -p $(COMPDB_DIR)
	$(MAKE) COMPDB=1 all
	$(PYTHON) tools/merge_compdb.py $(COMPDB_DIR) compile_commands.json

# 显式生成嵌入源；普通 make 在 TTF 变化时也会自动重新生成。
font: $(FONT_C)

clean:
	rm -rf build
	rm -f compile_commands.json

ifeq ($(UNAME_S),Darwin)
APP       := build/pacman-gui.app
APP_BIN   := $(APP)/Contents/MacOS/pacman-gui
ICONSET   := build/pacman-gui.iconset
ICNS      := $(APP)/Contents/Resources/pacman-gui.icns
PLIST_SRC := packaging/macos/Info.plist

app: $(APP_BIN) $(ICNS) $(PLIST_SRC)
	@mkdir -p $(APP)/Contents/Resources
	cp $(PLIST_SRC) $(APP)/Contents/Info.plist
	@rm -rf $(APP)/Contents/Resources/assets
	cp -R assets $(APP)/Contents/Resources/assets
	@if [ -d locale ]; then rm -rf $(APP)/Contents/Resources/locale; cp -R locale $(APP)/Contents/Resources/locale; fi
	@echo "已生成 $(APP)"

$(APP_BIN): $(TARGET)
	@mkdir -p $(dir $@)
	cp $< $@

# 优先用高分辨率母版；老检出里没有 icon-1024.png 时退回 256px 图标
ICON_SOURCE := $(if $(wildcard assets/icon-1024.png),assets/icon-1024.png,assets/icon.png)

$(ICNS): $(ICON_SOURCE) tools/make_icns.sh
	@mkdir -p $(dir $@)
	sh tools/make_icns.sh $< $(ICONSET) $@

run-app: app
	open $(APP)
else
app run-app:
	@echo "make $@: 该目标只在 macOS 上可用" >&2
	@false
endif

-include $(DEPS)
