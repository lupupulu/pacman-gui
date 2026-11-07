#ifndef SDLGUI_THEME_H
#define SDLGUI_THEME_H

#include <stddef.h>

#ifndef SDL_TTF_H_
typedef struct TTF_Font TTF_Font;
#endif

#ifndef SDL_render_h_
typedef struct SDL_Texture SDL_Texture;
#endif

/* 颜色在主题切片 color[] 数组里的下标 */
typedef enum {
    COLOR_MAIN=0,
    COLOR_SECONDARY,
    COLOR_BUTTON,
    COLOR_BUTTON_CONFIRM,
    COLOR_BUTTON_HIGHLIGHT,
    COLOR_SWITCH_OFF,     /* 开关关闭轨道 */
    COLOR_SWITCH_ON,      /* 开关打开轨道 */
    COLOR_SWITCH_KNOB     /* 开关旋钮图标着色 */
}theme_color_t;

/* 文本级别：同时用作 fonts[]、text_size[]、text_color[] 的下标 */
typedef enum {
    TEXT_TITLE=0,
    TEXT_SUBTITLE,
    TEXT_CONTEXT
}theme_text_t;

/* 内置图标下标。其他图标通过 add_icon_rule() 动态分配。 */
typedef enum {
    ICON_LOST=0,
    ICON_SWITCH=1,   /* 开关旋钮 */
}theme_icon_t;

typedef struct {
    char *name;
    unsigned *color;
    size_t color_size;
    unsigned *text_color;
    size_t text_color_size;

    unsigned divider_color;   /* 分割线颜色，单个 0xRRGGBBAA */
}theme_slice_t;

typedef struct {
    char *name;         /* NULL 表示内联主题，非 NULL 表示从配置文件加载的主题 */
    char *version;
    theme_slice_t *now_slice;

    char **fonts;       /* 字体族名；NULL 项表示使用内嵌字体 */
    size_t font_size;

    unsigned *text;
    size_t text_size;

    char *icon_path;
    SDL_Texture **icons;
    size_t icon_size;

    float item_height;
    unsigned long button_animation_time;
    unsigned long switch_animation_time;
    float button_height;
    float switch_width;      /* 开关总长 */
    float switch_icon_height;  /* 开关旋钮图标边长；0 表示跟随开关高度 */
    float rect_round_corner;

    theme_slice_t *slices;
    size_t slice_size;
}theme_t;

/* 生命周期函数，供 sdlgui_init() / sdlgui_quit() 调用。
 * theme_init() 统一做两件事：预热默认字体、从 icon_default_rules 初始化图标规则。 */
int theme_init(void);
int theme_load_default_font(void);
void theme_quit(void);

/* 读取主题配置；配置里带 fonts 时通过 obtain_font() 取字体，需在 sdlgui_init() 之后调用 */
void theme_set(const char *path,const char *file);

/* 按切片名切换当前主题；找不到返回 0 */
int theme_select_slice(const char *name);
/* 根据 SDL_GetSystemTheme() 选择 Dark/Bright；未知时保持当前切片 */
int theme_select_system(void);

unsigned get_color(theme_color_t index);
unsigned get_text_color(theme_text_t index);
unsigned get_divider_color(void);
const char *get_font(theme_text_t index);   /* 返回字体族名，不是 TTF_Font */
float get_text_size(theme_text_t index);
/* 取图标；返回的纹理归主题所有，主题切换/退出后失效。
 * 主题没有该图标时，switch 回退到内嵌旋钮，其余回退到 lost 问号。 */
SDL_Texture *get_icon(theme_icon_t index);
const theme_t *get_theme(void);
const theme_slice_t *get_theme_slice(void);


/* 按字体族名和字号取字体：命中缓存直接返回；找不到时返回内嵌字体。
 * 返回的 TTF_Font 归缓存所有，调用方不要 TTF_CloseFont()。 */
TTF_Font *obtain_font(const char *name,float ptsize);

/* 只负责查找并加载图标：在 path 下找 name.png（name 已带 .png 时不重复添加）。
 * path 为 NULL 时支持内置的 "lost" 和 "switch"。返回的纹理由调用方释放。 */
SDL_Texture *load_icon(const char *path,const char *name);
/* 按 name 有序插入规则；index<0 时自动分配新下标。
 * name 必须匹配 [A-Za-z_][A-Za-z0-9_]*（数字不能作首字母）。
 * 返回 name 对应的下标，name 已存在、非法或失败返回 -1。 */
long add_icon_rule(const char *name,long index);
/* 删除一条规则；返回 0-完成 1-无条目 */
int del_icon_rule(const char *name,theme_icon_t index);
/* 按 name 查规则；找不到时返回 ICON_LOST。 */
unsigned long find_icon(const char *name);


#endif
