#ifndef SDLGUI_H
#define SDLGUI_H

#include <stddef.h>

#ifndef SDL_render_h_
typedef struct SDL_Renderer SDL_Renderer;
#endif
#ifndef SDL_video_h_
typedef struct SDL_Window SDL_Window;
#endif

typedef struct layout_t layout_t;

void sdlgui_init(const char *title,int width,int height,const char *iconpath);
void sdlgui_quit(void);

void set_window(SDL_Window *w);
SDL_Window *get_window(void);
void set_renderer(SDL_Renderer *r);
SDL_Renderer *get_renderer(void);

void update_root(void);
void resize_root(int w,int h);
const layout_t *get_root(void);

float get_root_w(void);
float get_root_h(void);


typedef struct {
    unsigned horizon:2;  // 0:不在水平上对齐 1:向左对齐 2:向右对齐 3:水平对齐中央
    unsigned vertical:2; // 0:不在竖直上对齐 1:向下对齐 2:向上对齐 3:竖直对齐中央
    unsigned width:2;    // 0:自定宽度 1:以指定倍数的父布局宽度 2:指定偏移量的父布局宽度
    unsigned height:2;   // 0:自定高度 1:以指定倍数的父布局高度 2:指定偏移量的父布局高度
    float horizon_offset;  // 水平对齐偏移量，正数向右，负数向左
    float vertical_offset; // 竖直对齐偏移量，正数向下，负数向上
    float width_offset;
    float height_offset;
}align_t;

#define make_align(hor_,ver_,w_,h_,hor_off,ver_off,w_off,h_off) \
    ((align_t){ \
        .horizon=hor_,.vertical=ver_,.width=w_,.height=h_, \
        .horizon_offset=hor_off,.vertical_offset=ver_off,.width_offset=w_off,.height_offset=h_off  \
    })

typedef void(*update_func)(void *l);
typedef void(*draw_func)(void *l);
typedef void(*clear_func)(void *l);
typedef struct layout_t{
    int enable:1; // 是否启用，禁用时，不更新此布局
    int son_resizable:1; // sons是否可以被realloc
    int need_free:1; // 是否需要free释放

    align_t align_type;
    layout_t *parent; // 对齐目标
    float width;
    float height;
    float x;
    float y;

    layout_t **sons;  // 依赖此布局的子布局
    size_t son_size;

    draw_func draw;
    update_func update;
    clear_func clear;
}layout_t;

void *create_layout(size_t siz,layout_t *parent,float w,float h,draw_func draw,update_func update,clear_func clear,align_t align);
void delete_layout(layout_t *l);

void update_sons(void *l);
void update_layout_only(void *l); // 仅更新布局
void update_layout(void *l);


typedef struct {
    layout_t l;
    unsigned color;
}rect_layout_t; // 测试用矩形布局
void draw_rect(void *l);



#endif