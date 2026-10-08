# ============================================================
#  SDL3 项目模板 Makefile
#    make        编译到 build/pacman-gui
#    make run    编译并运行
#    make debug  带 ASan/UBSan 与调试信息编译
#    make clean  清理构建产物
# ============================================================

TARGET  := build/pacman-gui
SRCDIR  := src
SRCS    := $(wildcard $(SRCDIR)/*.c)
OBJS    := $(patsubst $(SRCDIR)/%.c,build/obj/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

CC      ?= cc
PKGCONF ?= pkg-config

# 依赖库：SDL3 必选；要用 SDL3_ttf / SDL3_image / SDL3_mixer 时加进这个列表
PKGS    := sdl3

CFLAGS  += -std=c99 -Wall -Wextra -Wshadow -O2 -g -MMD -MP
CFLAGS  += $(shell $(PKGCONF) --cflags $(PKGS))
LDLIBS  += $(shell $(PKGCONF) --libs $(PKGS))

.PHONY: all run debug clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

build/obj/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# 内存/未定义行为检查版本（注意会明显变慢）
debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c99 -Wall -Wextra -O0 -g3 -fsanitize=address,undefined" \
	        LDFLAGS="-fsanitize=address,undefined"

clean:
	rm -rf build

-include $(DEPS)
