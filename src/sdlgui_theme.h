#ifndef SDLGUI_THEME_H
#define SDLGUI_THEME_H

#include <stddef.h>

#ifndef SDL_TTF_H_
typedef struct TTF_Font TTF_Font;
#endif

#define COLOR_MAIN       0
#define COLOR_SECONDARY  1
#define COLOR_HIGHLIGHT  2
#define COLOR_BUTTON     3

#define TEXT_TITLE    0
#define TEXT_SUBTITLE 1
#define TEXT_CONTEXT  2

typedef struct {
    char *name;
    unsigned *color;
    size_t color_size;
    unsigned *text_color;
    size_t text_color_size;
}theme_slice_t;

typedef struct {
    char *name;         /* NULL 表示内联主题，非 NULL 表示从配置文件加载的主题 */
    char *version;
    theme_slice_t *now_slice;

    TTF_Font **fonts;   /* 已加载的字体族；默认是 sdlgui_init() 填入的嵌入字体，theme_set 可按 fonts 配置替换 */
    size_t font_size;

    unsigned *text;
    size_t text_size;

    float item_height;
    float rect_round_corner;

    theme_slice_t *slices;
    size_t slice_size;
}theme_t;

/* 生命周期函数，供 sdlgui_init() / sdlgui_quit() 调用 */
int theme_load_default_font(void);
void theme_quit(void);

/* 读取主题配置；配置里带 fonts 时会用 fontconfig 打开字体，需在 sdlgui_init() 之后调用 */
void theme_set(const char *path,const char *file);
void set_theme(const char *path,const char *file);   /* theme_set 的别名 */

/* 按字体族名用 fontconfig 找到字体文件，再按当前主题的基础字号打开 */
TTF_Font *create_font(const char *name);

unsigned get_color(size_t n);
unsigned get_text_color(size_t n);
TTF_Font *get_font(size_t n);
float get_text_size(size_t n);
float get_height(void);
float get_rrect(void);

#endif
