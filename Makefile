# ============================================================
#  SDL3 项目模板 Makefile
#    make        编译到 build/pacman-gui
#    make run    编译并运行
#    make debug  带 ASan/UBSan 与调试信息编译
#    make app    （macOS）生成 build/pacman-gui.app
#    make compdb 生成 compile_commands.json（VS Code / clangd 用）
#    make test-input 编译并运行输入框测试程序
#    make OPT=-O3 LTO=1  开启链接时优化（也支持 -Os/-O2）
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

# 内置图标：只把 lost 图标嵌入可执行文件；开关旋钮作为主题 PNG 资源加载。
LOST_ICON_C   := build/lost_icon.c
LOST_ICON_OBJ := build/obj/lost_icon.o

OBJS    := $(patsubst $(SRCDIR)/%.c,build/obj/%.o,$(SRCS)) $(FONT_OBJ) $(LOST_ICON_OBJ)
DEPS    := $(OBJS:.o=.d)

CC       ?= cc
PKGCONF  ?= pkg-config
PYTHON   ?= python3

UNAME_S := $(shell uname -s)
HAVE_PKGCONFIG := $(shell command -v $(PKGCONF) >/dev/null 2>&1 && echo 1)

# Homebrew 前缀：PATH 里没有 brew 时退回常见安装位置
BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null || (test -d /opt/homebrew && echo /opt/homebrew || echo /usr/local))

OPT      ?= -O2
LTO      ?= 0

CFLAGS  += -std=c99 -Wall -Wextra -Wshadow $(OPT) -g -MMD -MP
LDFLAGS +=
LDLIBS  +=

ifeq ($(LTO),1)
  CFLAGS  += -flto
  LDFLAGS += -flto
endif

# OPT/LTO/BUILD 改变时自动重建，避免把不同优化级别的 .o 混在一起。
BUILD_FLAGS      := $(CC)|$(OPT)|$(LTO)|$(BUILD)
BUILD_FLAGS_FILE := build/.build-flags
BUILD_FLAGS_OLD  := $(shell cat $(BUILD_FLAGS_FILE) 2>/dev/null)
ifneq ($(BUILD_FLAGS_OLD),$(BUILD_FLAGS))
  $(info build flags changed; rebuilding objects)
  $(shell mkdir -p build; rm -rf build/obj; printf '%s\n' '$(BUILD_FLAGS)' > $(BUILD_FLAGS_FILE))
endif

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
COMPDB_ICON_FLAG = -MJ $(COMPDB_DIR)/lost_icon.json
else
COMPDB_FLAG      =
COMPDB_FONT_FLAG =
COMPDB_ICON_FLAG =
endif

# 内存/未定义行为检查版本（注意会明显变慢）
ifeq ($(BUILD),debug)
CFLAGS  += -O0 -g3 -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif

.PHONY: all run debug clean font app run-app compdb test-input

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# 输入框测试程序：不包含 src/main.o，使用自己的 main
TEST_INPUT      := build/input-test
TEST_INPUT_SRC  := tests/input_test.c
TEST_INPUT_OBJ  := build/obj/input_test.o
TEST_INPUT_OBJS := $(filter-out build/obj/main.o,$(OBJS))

$(TEST_INPUT_OBJ): $(TEST_INPUT_SRC)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

$(TEST_INPUT): $(TEST_INPUT_OBJ) $(TEST_INPUT_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(TEST_INPUT_OBJ) $(TEST_INPUT_OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

test-input: $(TEST_INPUT)
	./$(TEST_INPUT)

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
