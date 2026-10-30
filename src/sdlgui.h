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
void layout_update(void *l);
void layout_mouse(void *l);


typedef struct {
    layout_t l;
    unsigned color;
}layout_rect_t; // 测试用矩形布局
void rect_draw(void *l);
layout_rect_t *create_layout_rect(void *parent,float w,float h,align_t align);




typedef void(*animate_func)(void*);

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF
}draft_t;

typedef struct {
    draw_func draw;
    clear_func clear;
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
    clear_func clear;
    SUBLAYOUT_DEF
    float points[3][2];   /* 以包围盒左上角为原点的局部坐标 */
    unsigned color;
    unsigned border_color;
}draft_triangle_t;

draft_triangle_t *draft_create_triangle(align_t align,float *vertex,unsigned color,unsigned border_color);
void draft_draw_triangle(void *l);

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF
    float r;  // 圆角半径
    unsigned color;
    unsigned border_color;
}draft_rrect_t;

draft_rrect_t *draft_create_rrect(align_t align,float w,float h,float r,unsigned color,unsigned border_color);
void draft_draw_rrect(void *l);

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF

    int can_enter:1; // 允许多行显示
    int can_change:1; // 显示内容可以发生变化

    TTF_Font *font;    // 行高由 TTF_GetFontHeight(font) 在绘制时读取
    char *context;    // 借用调用方的字符串，create 不复制也不负责释放

    float width_limit;
    float height_limit;

    unsigned color;

    SDL_Texture *cache;   // can_change=0 时的整段缓存纹理
    float cache_width;
    float cache_height;
}draft_text_t;

draft_text_t *draft_create_text(align_t align,TTF_Font *font,char *context,float wl,float hl,unsigned color);
void draft_draw_text(void *l);
void draft_text_clear(draft_text_t *l);   // 释放 can_change=0 的缓存纹理

typedef struct {
    draw_func draw;
    clear_func clear;
    SUBLAYOUT_DEF

    SDL_Texture *tex;
}draft_texture_t;

draft_texture_t *draft_create_picture_from_memory(align_t align,const char *file,size_t size);
draft_texture_t *draft_create_picture(align_t align,const char *path);
void draft_draw_texture(void *l);
void draft_texture_clear(draft_texture_t *l);   // 释放贴图

typedef struct {
    layout_t l;

    int draft_resizable:1;
    draft_t **drafts;
    size_t draft_size;

    animate_func animation;
    size_t timer;  // 计时器
    int status;  // 状态，0为不刷新动画
}layout_animation_t;

/* 可变参数是若干 draft 指针，以 NULL 结尾；draft 所有权交给 animation，
 * clear 时会释放 draft 及其缓存纹理/贴图。 */
layout_animation_t *create_layout_animation(void *parent,animate_func animation,...);

void animation_draw(void *l);
void animation_clear(void *l);






/* 每种判定框一个 check 函数，签名与 bbox_check_func 一致：
   参数是子布局自己和鼠标的窗口坐标；子布局的 x/y 已经是窗口绝对坐标 */
typedef int(*bbox_check_func)(void*,float x,float y);

typedef struct {
    bbox_check_func check;
    SUBLAYOUT_DEF
}bbox_t; // 判定框公共前缀

typedef bbox_t bbox_default_t; // 默认判定框（矩形/圆）

bbox_default_t *bbox_create_default(bbox_check_func check,align_t align,float w,float h);
int bbox_check_rect(void *sl,float x,float y);   /* 矩形 */
/* 圆形，判定框用 bbox_default_t：宽高不等时圆要选一个直径
   align_w：高向宽看齐，直径 = width
   align_h：宽向高看齐，直径 = height
   两者圆心都取判定框中心 */
int bbox_check_circle_w(void *sublayout,float x,float y);
int bbox_check_circle_h(void *sublayout,float x,float y);
#define bbox_check_circle bbox_check_circle_h


typedef struct {
    bbox_check_func check;
    SUBLAYOUT_DEF
    float points[3][2];   /* 以包围盒左上角为原点的局部坐标 */
}bbox_triangle_t;

bbox_triangle_t *bbox_create_triangle(align_t align,float *vertex);
int bbox_check_triangle(void *sublayout,float x,float y);  /* 三角形 */


typedef struct {
    bbox_check_func check;
    SUBLAYOUT_DEF
    float r;  // 圆角半径
}bbox_rrect_t;

bbox_rrect_t *bbox_create_rrect(align_t align,float w,float h,float r);
int bbox_check_rrect(void *sublayout,float x,float y);     /* 圆角矩形 */


typedef void(*button_callback_func)(void *data);
typedef struct {
    layout_t l;
    int sublayout_resizable:1;
    unsigned left_last_state:1; // 鼠标左键上次状态
    unsigned need_bounce:2;     // 是否等到鼠标回弹再触发 0:否 1:无论回弹时在哪，回弹即触发 2:只有在子布局内回弹才触发

    bbox_t **bboxes;
    size_t bboxes_size;

    button_callback_func callback;
    void *data;
}layout_button_t;

/* 可变参数是若干 bbox 指针，以 NULL 结尾；所有权交给按钮，button_clear() 会释放。 */
layout_button_t *create_layout_button(void *parent,float w,float h,align_t align,button_callback_func callback,void *data,...);



void button_clear(void *l);
void button_mouse(void *l);


#endif