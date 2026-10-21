#ifndef SDLGUI_H
#define SDLGUI_H

#include <stddef.h>
#include "sdlgui_theme.h"

#ifndef SDL_render_h_
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
#endif
#ifndef SDL_video_h_
typedef struct SDL_Window SDL_Window;
#endif

typedef struct layout_t layout_t;   /* get_root() 等在定义之前就要用到 */

void sdlgui_init(const char *title,int width,int height,const char *iconpath);
void sdlgui_quit(void);

void set_window(SDL_Window *w);
SDL_Window *get_window(void);
void set_renderer(SDL_Renderer *r);
SDL_Renderer *get_renderer(void);

void set_mouse_pos(float x,float y);
float get_mouse_x(void);
float get_mouse_y(void);

/* 五个鼠标按键状态：1=按下，0=未按下（左 中 右 侧键X1 侧键X2） */
void set_mouse_button(int left,int mid,int right,int x1,int x2);
int get_mouse_left(void);
int get_mouse_mid(void);
int get_mouse_right(void);
int get_mouse_x1(void);
int get_mouse_x2(void);

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
typedef void(*mouse_func)(void *l);

typedef struct sublayout_t {
    struct sublayout_t *parent;   /* 自引用必须带 tag，否则 typedef 名字在这里还不可见 */
    align_t align;
    float x;
    float y;
    float width;
    float height;
}sublayout_t;
#define SUBLAYOUT_DEF    \
    sublayout_t *parent; \
    align_t align;       \
    float x;             \
    float y;             \
    float width;         \
    float height;

typedef struct layout_t{
    SUBLAYOUT_DEF

    int enable:1; // 是否启用，禁用时，不更新此布局
    int active:1; // 是否活跃，活跃时更新鼠标事件
    int son_resizable:1; // sons是否可以被realloc
    int need_free:1; // 是否需要free释放

    layout_t **sons;  // 依赖此布局的子布局
    size_t son_size;

    draw_func draw;
    update_func update;
    clear_func clear;
    mouse_func mouse;
}layout_t;

void *create_layout(size_t siz,layout_t *parent,float w,float h,draw_func draw,update_func update,clear_func clear,mouse_func mouse,align_t align);
void delete_layout(layout_t *l);

void update_sons(void *l);
void update_layout_only(void *l); // 仅更新布局
void update_layout(void *l);
void mouse_update_sons(void *l);


typedef struct {
    layout_t l;
    unsigned color;
}layout_rect_t; // 测试用矩形布局
void rect_draw(void *l);
layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align);


typedef int(*check_func)(void*,float x,float y);

typedef struct {
    check_func check;
    SUBLAYOUT_DEF
}default_sublayout_t; // 子布局，用作判定框等
typedef struct {
    check_func check;
    float points[3][2];
}triangle_sublayout_t;
typedef struct {
    check_func check;
    SUBLAYOUT_DEF
    float r;  // 圆角半径
}rrect_sublayout_t;

/* 每种判定框一个 check 函数，签名与 check_func 一致：
   参数是子布局自己和鼠标的窗口坐标；子布局的 x/y 已经是窗口绝对坐标 */
int check_rect(void *sublayout,float x,float y);   /* 矩形 */
int check_triangle(void *sublayout,float x,float y);  /* 三角形 */
int check_rrect(void *sublayout,float x,float y);     /* 圆角矩形 */

/* 圆形，判定框用 default_sublayout_t：宽高不等时圆要选一个直径
   align_w：高向宽看齐，直径 = width
   align_h：宽向高看齐，直径 = height
   两者圆心都取判定框中心 */
int check_circle_w(void *sublayout,float x,float y);
int check_circle_h(void *sublayout,float x,float y);
#define check_circle check_circle_h


typedef void(*animate_func)(void*);

typedef struct {
    draw_func draw;
    SUBLAYOUT_DEF
    unsigned color;
    unsigned border_color;
}draft_default_t;

draft_default_t *draft_create_default(draw_func draw,align_t align,float w,float h,unsigned color,unsigned border_color);
void draft_draw_rect(void *l);
void draft_draw_circle_w(void *l);
void draft_draw_circle_h(void *l);

typedef struct {
    draw_func draw;
    SUBLAYOUT_DEF
    float points[3][2];   /* 以包围盒左上角为原点的局部坐标 */
    unsigned color;
    unsigned border_color;
}draft_triangle_t;

draft_triangle_t *draft_create_triangle(align_t align,float *vertex,unsigned color,unsigned border_color);
void draft_draw_triangle(void *l);

typedef struct {
    draw_func draw;
    SUBLAYOUT_DEF
    float r;  // 圆角半径
    unsigned color;
    unsigned border_color;
}draft_rrect_t;

draft_rrect_t *draft_create_rrect(align_t align,float w,float h,float r,unsigned color,unsigned border_color);
void draft_draw_rrect(void *l);

typedef struct {
    draw_func draw;
    SUBLAYOUT_DEF

    int can_enter:1; // 允许多行显示
    int can_change:1; // 显示内容可以发生变化

    char *context;    /* 借用调用方的字符串，create 不复制也不负责释放 */

    float width_limit;
    float height_per_line;
    float height_limit;

    unsigned color;
}draft_text_t;

draft_text_t *draft_create_text(align_t align,char *context,float wl,float hl,float hpl,unsigned color);
void draft_draw_text(void *l);

typedef struct {
    draw_func draw;
    SUBLAYOUT_DEF

    SDL_Texture *tex;
}draft_texture_t;

draft_texture_t *draft_create_picture_from_memory(align_t align,const char *file,size_t size);
draft_texture_t *draft_create_picture(align_t align,const char *path);
void draft_draw_texture(void *l);

typedef struct {
    layout_t l;

    int draft_resizable:1;
    draw_func **drafts;
    size_t draft_size;

    animate_func animation;
    double degree;  // 动画完成程度
}layout_draw_t;


typedef void(*button_callback_func)(void *data);
typedef struct {
    layout_t l;
    int sublayout_resizable:1;
    unsigned left_last_state:1; // 鼠标左键上次状态
    unsigned need_bounce:2;     // 是否等到鼠标回弹再触发 0:否 1:无论回弹时在哪，回弹即触发 2:只有在子布局内回弹才触发

    check_func **sublayouts;
    size_t sublayouts_size;

    button_callback_func callback;
    void *data;
}layout_button_t;

layout_button_t *create_layout_button(void *parent,float w,float h,align_t align,int need_bounce,button_callback_func callback,void *data);

void button_clear(void *l);
void button_mouse(void *l);


#endif