# ============================================================
#  SDL3 项目模板 Makefile
#    make        编译到 build/pacman-gui
#    make run    编译并运行
#    make debug  带 ASan/UBSan 与调试信息编译
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

OBJS    := $(patsubst $(SRCDIR)/%.c,build/obj/%.o,$(SRCS)) $(FONT_OBJ)
DEPS    := $(OBJS:.o=.d)

CC      ?= cc
PKGCONF ?= pkg-config

# 依赖库：SDL3 必选；要用 SDL3_ttf / SDL3_image / SDL3_mixer 时加进这个列表
PKGS    := sdl3 sdl3-ttf fontconfig

CFLAGS  += -std=c99 -Wall -Wextra -Wshadow -O2 -g -MMD -MP
CFLAGS  += $(shell $(PKGCONF) --cflags $(PKGS))
LDLIBS  += $(shell $(PKGCONF) --libs $(PKGS))

.PHONY: all run debug clean font

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

build/obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(FONT_C): $(FONT_TTF) tools/ttf_to_c.py
	@mkdir -p $(dir $(FONT_C))
	python3 tools/ttf_to_c.py $(FONT_TTF) -o $(FONT_C) --symbol $(FONT_SYM)

$(FONT_OBJ): $(FONT_C)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $(FONT_C) -o $@

run: $(TARGET)
	./$(TARGET)

# 内存/未定义行为检查版本（注意会明显变慢）
debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c99 -Wall -Wextra -O0 -g3 -fsanitize=address,undefined $(shell $(PKGCONF) --cflags $(PKGS))" \
	        LDFLAGS="-fsanitize=address,undefined"

# 显式生成嵌入源；普通 make 在 TTF 变化时也会自动重新生成。
font: $(FONT_C)

clean:
	rm -rf build

-include $(DEPS)
