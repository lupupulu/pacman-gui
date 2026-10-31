#ifndef SDLGUI_THEME_H
#define SDLGUI_THEME_H

#include <stddef.h>

#ifndef SDL_TTF_H_
typedef struct TTF_Font TTF_Font;
#endif

#define COLOR_MAIN             0
#define COLOR_SECONDARY        1
#define COLOR_BUTTON           2
#define COLOR_BUTTON_CONFIRM   3
#define COLOR_BUTTON_HIGHLIGHT 4


#define TEXT_TITLE    0
#define TEXT_SUBTITLE 1
#define TEXT_CONTEXT  2

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

    float item_height;
    unsigned long button_animation_time;
    float button_height;
    float rect_round_corner;

    theme_slice_t *slices;
    size_t slice_size;
}theme_t;

/* 生命周期函数，供 sdlgui_init() / sdlgui_quit() 调用 */
int theme_load_default_font(void);
void theme_quit(void);

/* 读取主题配置；配置里带 fonts 时通过 obtain_font() 取字体，需在 sdlgui_init() 之后调用 */
void theme_set(const char *path,const char *file);

/* 按切片名切换当前主题；找不到返回 0 */
int theme_select_slice(const char *name);
/* 根据 SDL_GetSystemTheme() 选择 Dark/Bright；未知时保持当前切片 */
int theme_select_system(void);

unsigned get_color(size_t n);
unsigned get_text_color(size_t n);
unsigned get_divider_color(void);
const char *get_font(size_t n);   /* 返回字体族名，不是 TTF_Font */
float get_text_size(size_t n);
const theme_t *get_theme(void);
const theme_slice_t *get_theme_slice(void);


/* 按字体族名和字号取字体：命中缓存直接返回；找不到时返回内嵌字体。
 * 返回的 TTF_Font 归缓存所有，调用方不要 TTF_CloseFont()。 */
TTF_Font *obtain_font(const char *name,float ptsize);


#endif
