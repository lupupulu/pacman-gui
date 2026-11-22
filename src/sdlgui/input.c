/*
 * Copyright (C) 2026 Zhang Cw and pacman-gui contributors
 *
 * This file is part of pacman-gui.
 *
 * pacman-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pacman-gui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with pacman-gui.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "input.h"
#include "sdlgui.h"
#include "sdlgui_internal.h"
#include "theme.h"
#include "CDS/darray.h"

static layout_input_t *focused_input=NULL;
static layout_input_t *input_hover=NULL;
static SDL_Cursor *input_text_cursor=NULL;
static int input_last_left=0;

static int input_contains_mouse(const layout_t *l){
    if(l==NULL){
        return 0;
    }
    float x=get_mouse_x();
    float y=get_mouse_y();
    return x>=l->x&&x<=l->x+l->width&&y>=l->y&&y<=l->y+l->height;
}

static unsigned input_color_gradient(unsigned target,unsigned origin,size_t timer,size_t whole){
    if(whole==0||timer>=whole){
        return target;
    }
    if(timer==0){
        return origin;
    }

    float t=(float)timer/(float)whole;
    unsigned tr=(target>>24)&0xFF, tg=(target>>16)&0xFF, tb=(target>>8)&0xFF, ta=target&0xFF;
    unsigned or=(origin>>24)&0xFF, og=(origin>>16)&0xFF, ob=(origin>>8)&0xFF, oa=origin&0xFF;

    unsigned r=(unsigned)((float)or+((float)tr-(float)or)*t+0.5f);
    unsigned g=(unsigned)((float)og+((float)tg-(float)og)*t+0.5f);
    unsigned b=(unsigned)((float)ob+((float)tb-(float)ob)*t+0.5f);
    unsigned a=(unsigned)((float)oa+((float)ta-(float)oa)*t+0.5f);
    return (r<<24)|(g<<16)|(b<<8)|a;
}

static float input_theme_height(void){
    float height=get_theme()->item_height;
    if(height<=0.0f){
        height=get_theme()->button_height;
    }
    if(height<=0.0f){
        height=32.0f;
    }
    return height;
}

/* ---- input layout：鼠标形状/焦点/键盘编辑，文字 draft 只绘制文字 ---- */

static void input_sync_ime_area(layout_input_t *input);

#define INPUT_SCROLL_STEP 48.0f
/* 光标离文字区左右边缘的最小留白，滚动跟随光标时使用 */
#define INPUT_CARET_MARGIN 2.0f

/* 截断到 max_bytes，并保证不切断 UTF-8 字符。max_bytes=0 表示不限制。 */
static size_t input_limit_prefix(const char *text,size_t length,size_t max_bytes){
    if(text==NULL||max_bytes==0||length<=max_bytes){
        return length;
    }
    size_t limit=max_bytes;
    while(limit>0&&((unsigned char)text[limit]&0xC0)==0x80){
        limit--;
    }
    return limit;
}

static void input_measure_text(layout_input_t *input){
    if(input==NULL){
        return;
    }
    input->text_width=0.0f;
    if(input->font==NULL||input->text==NULL||input->length==0){
        return;
    }

    int width=0;
    int height=0;
    if(TTF_GetStringSize(input->font,input->text,input->length,&width,&height)&&width>0){
        float scale=sdlgui_get_scale();
        if(scale<=0.0f){
            scale=1.0f;
        }
        input->text_width=(float)width/scale;
    }else if(input->text_draft!=NULL){
        input->text_width=input->text_draft->width;
    }
}

static void input_clamp_scroll(layout_input_t *input){
    if(input==NULL){
        return;
    }
    float max_scroll=input->text_width-input->l.width;
    if(max_scroll<0.0f){
        max_scroll=0.0f;
    }
    if(input->text_scroll<0.0f){
        input->text_scroll=0.0f;
    }else if(input->text_scroll>max_scroll){
        input->text_scroll=max_scroll;
    }
}

static void input_position_text(layout_input_t *input){
    if(input==NULL||input->text_draft==NULL){
        return;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    float line_height=input->font!=NULL?(float)TTF_GetFontHeight(input->font)/scale:0.0f;

    input->text_draft->x=input->l.x-input->text_scroll;
    input->text_draft->y=input->l.y;
    if(input->text_draft->height>0.0f){
        input->text_draft->y+=(input->l.height-input->text_draft->height)/2.0f;
    }else if(line_height>0.0f){
        input->text_draft->y+=(input->l.height-line_height)/2.0f;
    }
}

static int input_trim_to_limit(layout_input_t *input){
    if(input==NULL||input->max_bytes==0||input->length<=input->max_bytes){
        return 0;
    }

    size_t length=input_limit_prefix(input->text,input->length,input->max_bytes);
    input->length=length;
    input->text[length]='\0';
    if(input->cursor>length){
        input->cursor=length;
    }
    return 1;
}

static void input_rebuild_text(layout_input_t *input){
    if(input==NULL){
        return;
    }

    if(input->text_draft!=NULL){
        input->text_draft->context=input->text;
        input->text_draft->can_enter=0;
        input->text_draft->can_change=0;
        draft_text_clear(input->text_draft);
        draft_text_rebuild_cache(input->text_draft);
    }

    input_measure_text(input);
    input_clamp_scroll(input);
    input_position_text(input);
}

static int input_reserve(layout_input_t *input,size_t need){
    if(input==NULL){
        return 0;
    }
    if(need<=input->capacity){
        return 1;
    }

    size_t capacity=input->capacity>0?input->capacity:32;
    while(capacity<need){
        if(capacity>SIZE_MAX/2){
            capacity=need;
            break;
        }
        capacity*=2;
    }

    char *text=realloc(input->text,capacity);
    if(text==NULL){
        return 0;
    }
    input->text=text;
    input->capacity=capacity;
    return 1;
}

static size_t input_utf8_prev(const char *text,size_t at){
    if(text==NULL||at==0){
        return 0;
    }
    at--;
    while(at>0&&((unsigned char)text[at]&0xC0)==0x80){
        at--;
    }
    return at;
}

static size_t input_utf8_next(const char *text,size_t length,size_t at){
    if(text==NULL||at>=length){
        return length;
    }
    at++;
    while(at<length&&((unsigned char)text[at]&0xC0)==0x80){
        at++;
    }
    return at;
}

static int input_insert(layout_input_t *input,const char *text){
    if(input==NULL){
        return 0;
    }

    int trimmed=input_trim_to_limit(input);

    size_t add=(text!=NULL)?strlen(text):0;
    if(input->max_bytes>0&&add>0){
        size_t available=input->max_bytes-input->length;
        if(add>available){
            add=available>0?input_limit_prefix(text,add,available):0;
        }
    }

    if(add==0){
        if(trimmed){
            input_rebuild_text(input);
        }
        return 1;
    }

    if(!input_reserve(input,input->length+add+1)){
        return 0;
    }

    memmove(input->text+input->cursor+add,
        input->text+input->cursor,
        input->length-input->cursor+1);
    memcpy(input->text+input->cursor,text,add);
    input->length+=add;
    input->cursor+=add;
    input_rebuild_text(input);
    return 1;
}

static void input_clear(void *p){
    layout_input_t *input=p;
    if(input==NULL){
        return;
    }

    if(focused_input==input){
        layout_input_blur();
    }
    if(input_hover==input){
        SDL_SetCursor(SDL_GetDefaultCursor());
        input_hover=NULL;
    }

    if(input->text_draft!=NULL){
        draft_text_clear(input->text_draft);
        free(input->text_draft);
        input->text_draft=NULL;
    }

    free(input->text);
    input->text=NULL;
    input->length=0;
    input->capacity=0;
    input->cursor=0;
    input->text_width=0.0f;
    input->text_scroll=0.0f;
}

static void input_mouse(void *p){
    layout_input_t *input=p;
    if(input==NULL){
        return;
    }

    int inside=input_contains_mouse(&input->l);
    int left=get_mouse_left();

    if(inside){
        if(input_hover!=input){
            if(input_text_cursor==NULL){
                input_text_cursor=SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
            }
            if(input_text_cursor!=NULL){
                SDL_SetCursor(input_text_cursor);
            }
            input_hover=input;
        }
        if(left&&!input->last_left){
            layout_input_focus(input);
        }

        if(input->text_width>input->l.width&&input->l.width>0.0f){
            float wheel=get_mouse_wheel_x();
            if(wheel!=0.0f){
                input->text_scroll+=wheel*INPUT_SCROLL_STEP;
                input_clamp_scroll(input);
                sdlgui_consume_wheel();
                input_position_text(input);
                input_sync_ime_area(input);
            }
        }
    }else if(input_hover==input){
        SDL_SetCursor(SDL_GetDefaultCursor());
        input_hover=NULL;
    }

    input->last_left=left?1:0;
}

/* 光标前缀宽度：at 个字节的文字宽度（逻辑像素）。
 * TTF_GetStringSize(...,0,...) 会测量整段字符串，传入 0 会得到
 * “光标在末尾”的宽度，必须单独处理 at==0。 */
static float input_width_at(const layout_input_t *input,size_t at){
    if(input==NULL||input->font==NULL||input->text==NULL||at==0){
        return 0.0f;
    }

    int width=0;
    int height=0;
    if(!TTF_GetStringSize(input->font,input->text,at,&width,&height)||width<=0){
        return 0.0f;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    return (float)width/scale;
}

float layout_input_prefix_width(const layout_input_t *input){
    return input!=NULL?input_width_at(input,input->cursor):0.0f;
}

/* 把 IME/候选窗的位置同步到输入框和当前光标。
 * SDL3 的 SDL_SetTextInputArea() 会交给平台输入法使用；
 * rect 是窗口逻辑坐标，cursor 是相对 rect.x 的偏移。 */
static void input_sync_ime_area(layout_input_t *input){
    SDL_Window *w=get_window();
    if(input==NULL||!input->focused||w==NULL){
        return;
    }

    /* 把区域缩到光标所在的小矩形。macOS 的 SDL Cocoa 后端直接用这个
     * rect 决定候选窗位置，不读取 cursor 偏移；给整块输入框会让候选窗
     * 停在输入框左下角，所以这里传光标位置。 */
    float caret_x=input->l.x+layout_input_prefix_width(input)-input->text_scroll;
    if(caret_x<input->l.x){
        caret_x=input->l.x;
    }
    if(caret_x>input->l.x+input->l.width){
        caret_x=input->l.x+input->l.width;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    float line_height=input->font!=NULL?(float)TTF_GetFontHeight(input->font)/scale:0.0f;
    if(line_height<=0.0f){
        line_height=input->l.height;
    }
    if(line_height>input->l.height){
        line_height=input->l.height;
    }

    SDL_Rect area={
        (int)SDL_floorf(caret_x),
        (int)SDL_floorf(input->l.y+(input->l.height-line_height)/2.0f),
        1,
        (int)SDL_ceilf(line_height)
    };
    SDL_SetTextInputArea(w,&area,0);
}

void layout_input_focus(layout_input_t *input){
    if(input==NULL||focused_input==input){
        return;
    }

    if(focused_input!=NULL){
        focused_input->focused=0;
    }
    focused_input=input;
    input->focused=1;

    if(get_window()!=NULL){
        SDL_StartTextInput(get_window());
        input_sync_ime_area(input);
    }
}

void layout_input_blur(void){
    if(focused_input==NULL){
        return;
    }

    focused_input->focused=0;
    focused_input=NULL;

    if(get_window()!=NULL){
        SDL_SetTextInputArea(get_window(),NULL,0);
        SDL_StopTextInput(get_window());
    }
}

const char *layout_input_text(const layout_input_t *input){
    if(input==NULL||input->text==NULL){
        return "";
    }
    return input->text;
}

size_t layout_input_cursor(const layout_input_t *input){
    return input!=NULL?input->cursor:0;
}

int layout_input_is_focused(const layout_input_t *input){
    return input!=NULL&&input->focused;
}

void layout_input_scroll_into_view(layout_input_t *input){
    if(input==NULL||input->font==NULL||input->text==NULL){
        return;
    }

    /* 输入法编辑后立刻调用时 text_width 还是上一帧的值，先按当前文本重新测量，
     * 否则夹取范围会用到过期宽度，光标可能停在可见区外。 */
    input_measure_text(input);
    input_clamp_scroll(input);

    float view=input->l.width;
    if(view>0.0f){
        float prefix=layout_input_prefix_width(input);
        float caret=prefix-input->text_scroll;
        if(caret<INPUT_CARET_MARGIN){
            input->text_scroll=prefix-INPUT_CARET_MARGIN;
        }else if(caret>view-INPUT_CARET_MARGIN){
            input->text_scroll=prefix-(view-INPUT_CARET_MARGIN);
        }
        input_clamp_scroll(input);
    }

    input_position_text(input);
    input_sync_ime_area(input);
}

void layout_input_set_text(layout_input_t *input,const char *text){
    if(input==NULL){
        return;
    }

    size_t length=text!=NULL?strlen(text):0;
    length=input_limit_prefix(text,length,input->max_bytes);
    if(!input_reserve(input,length+1)){
        return;
    }

    /* text 可能指向 input->text 本身（set_max_bytes 会这样调用） */
    if(length>0){
        memmove(input->text,text,length);
    }
    input->text[length]='\0';
    input->length=length;

    if(input->cursor>length){
        input->cursor=length;
    }
    /* 新文本可能让原来的光标落到半个 UTF-8 字符里 */
    while(input->cursor>0&&((unsigned char)input->text[input->cursor]&0xC0)==0x80){
        input->cursor--;
    }

    input_rebuild_text(input);
    layout_input_scroll_into_view(input);
}

void layout_input_set_cursor(layout_input_t *input,size_t cursor){
    if(input==NULL||input->text==NULL){
        return;
    }

    if(cursor>input->length){
        cursor=input->length;
    }
    while(cursor>0&&((unsigned char)input->text[cursor]&0xC0)==0x80){
        cursor--;
    }

    input->cursor=cursor;
    layout_input_scroll_into_view(input);
}

void layout_input_set_cursor_at(layout_input_t *input,float x){
    if(input==NULL||input->text==NULL){
        return;
    }

    /* x 是窗口坐标，换算成相对文字起点的位置（已包含 text_scroll），
     * 然后找横坐标最接近的字符边界。 */
    float local=x-(input->l.x-input->text_scroll);
    size_t best=0;
    float best_distance=SDL_fabsf(local);
    size_t at=0;
    while(1){
        float width=input_width_at(input,at);
        float distance=SDL_fabsf(local-width);
        if(distance<best_distance){
            best=at;
            best_distance=distance;
        }
        if(at>=input->length||width>local){
            break;
        }
        at=input_utf8_next(input->text,input->length,at);
    }

    input->cursor=best;
    layout_input_scroll_into_view(input);
}

int layout_input_event(const SDL_Event *event){
    if(event==NULL||focused_input==NULL){
        return 0;
    }

    layout_input_t *input=focused_input;

    if(event->type==SDL_EVENT_TEXT_INPUT){
        input_insert(input,event->text.text);
        return 1;
    }
    if(event->type!=SDL_EVENT_KEY_DOWN){
        return 0;
    }

    switch(event->key.key){
    case SDLK_ESCAPE:
        layout_input_blur();
        return 1;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if(input->callback!=NULL){
            input->callback(layout_input_text(input),input->data);
        }
        return 1;
    case SDLK_BACKSPACE:
        if(input->cursor>0){
            size_t prev=input_utf8_prev(input->text,input->cursor);
            memmove(input->text+prev,
                input->text+input->cursor,
                input->length-input->cursor+1);
            input->length-=input->cursor-prev;
            input->cursor=prev;
            input_rebuild_text(input);
        }
        return 1;
    case SDLK_DELETE:
        if(input->cursor<input->length){
            size_t next=input_utf8_next(input->text,input->length,input->cursor);
            memmove(input->text+input->cursor,
                input->text+next,
                input->length-next+1);
            input->length-=next-input->cursor;
            input_rebuild_text(input);
        }
        return 1;
    case SDLK_LEFT:
        input->cursor=input_utf8_prev(input->text,input->cursor);
        return 1;
    case SDLK_RIGHT:
        input->cursor=input_utf8_next(input->text,input->length,input->cursor);
        return 1;
    case SDLK_HOME:
        input->cursor=0;
        return 1;
    case SDLK_END:
        input->cursor=input->length;
        return 1;
    case SDLK_V:
        if((event->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))!=0){
            char *clipboard=SDL_GetClipboardText();
            if(clipboard!=NULL){
                input_insert(input,clipboard);
                SDL_free(clipboard);
            }
            return 1;
        }
        break;
    default:
        break;
    }

    return 0;
}

static void input_update(void *p){
    layout_input_t *input=p;
    if(input==NULL){
        return;
    }

    layout_update(&input->l);
    if(input_trim_to_limit(input)){
        input_rebuild_text(input);
    }
    input_measure_text(input);
    input_clamp_scroll(input);
    input_position_text(input);
    input_sync_ime_area(input);
}

static void input_draw(void *p){
    layout_input_t *input=p;
    if(input==NULL||input->text_draft==NULL||renderer==NULL){
        return;
    }

    input_update(input);
    draft_draw_text(input->text_draft);
}

layout_input_t *create_layout_input(void *parent,float w,float h,align_t align,
    const char *text,TTF_Font *font,unsigned color,size_t max_bytes,
    input_callback_func callback,void *data){
    if(w<=0.0f||h<=0.0f||font==NULL){
        return NULL;
    }

    layout_input_t *input=(layout_input_t *)create_layout(
        sizeof(*input),
        parent,
        w,
        h,
        input_draw,
        input_update,
        input_clear,
        input_mouse,
        align
    );
    if(input==NULL){
        return NULL;
    }

    size_t full_length=text!=NULL?strlen(text):0;
    size_t length=input_limit_prefix(text,full_length,max_bytes);
    input->text=malloc(length+1);
    if(input->text==NULL){
        delete_layout((layout_t *)input);
        return NULL;
    }

    if(length>0){
        memcpy(input->text,text,length);
    }
    input->text[length]='\0';
    input->length=length;
    input->capacity=length+1;
    input->cursor=length;
    input->max_bytes=max_bytes;
    input->font=font;
    input->text_color=color;
    input->callback=callback;
    input->data=data;
    input->focused=0;
    input->last_left=0;

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    input->text_draft=draft_create_text(zero,font,input->text,0.0f,0.0f,color);
    if(input->text_draft==NULL){
        delete_layout((layout_t *)input);
        return NULL;
    }
    input->text_draft->parent=(sublayout_t *)&input->l;
    input->text_draft->can_enter=0;
    input->text_draft->can_change=0;
    input_rebuild_text(input);
    return input;
}


#define INPUT_ICON_CROSS 0
#define INPUT_ICON_SEARCH 1
#define INPUT_DEFAULT_BLINK 1000

static void input_widget_update(void *p);
static float input_theme_height(void);

typedef struct {
    float ax;
    float ay;
    float bx;
    float by;
}input_icon_stroke_t;

static int input_widget_contains(const layout_t *l,float x,float y){
    if(l==NULL){
        return 0;
    }
    return x>=l->x&&x<=l->x+l->width&&y>=l->y&&y<=l->y+l->height;
}

static char *input_widget_strdup(const char *text){
    if(text==NULL){
        return NULL;
    }
    size_t size=strlen(text)+1;
    char *copy=malloc(size);
    if(copy!=NULL){
        memcpy(copy,text,size);
    }
    return copy;
}

static void input_widget_draw_color(unsigned color){
    SDL_Renderer *r=get_renderer();
    if(r==NULL){
        return;
    }
    SDL_SetRenderDrawColor(r,
        (Uint8)((color>>24)&0xFF),
        (Uint8)((color>>16)&0xFF),
        (Uint8)((color>>8)&0xFF),
        (Uint8)(color&0xFF));
    SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
}

/* 点到圆头线段的最短距离，用于按覆盖率生成图标 */
static float input_icon_stroke_distance(float px,float py,const input_icon_stroke_t *stroke){
    float dx=stroke->bx-stroke->ax;
    float dy=stroke->by-stroke->ay;
    float length2=dx*dx+dy*dy;
    float t=0.0f;
    if(length2>0.0f){
        t=((px-stroke->ax)*dx+(py-stroke->ay)*dy)/length2;
        if(t<0.0f){
            t=0.0f;
        }else if(t>1.0f){
            t=1.0f;
        }
    }

    float offset_x=stroke->ax+t*dx-px;
    float offset_y=stroke->ay+t*dy-py;
    return SDL_sqrtf(offset_x*offset_x+offset_y*offset_y);
}

/* 在物理像素表面上画圆环和若干线段，输出白色 + 覆盖率 alpha。
 * 绘制时再用 SDL_SetTextureColorMod() 染色，这样一套纹理能适配任何主题色。 */
static void input_icon_paint(SDL_Surface *surface,int size,
    float ring_x,float ring_y,float ring_radius,
    const input_icon_stroke_t *strokes,int stroke_count,float half_width){
    for(int y=0;y<size;y++){
        for(int x=0;x<size;x++){
            float px=(float)x+0.5f;
            float py=(float)y+0.5f;
            float distance=1.0e9f;

            for(int i=0;i<stroke_count;i++){
                float d=input_icon_stroke_distance(px,py,&strokes[i]);
                if(d<distance){
                    distance=d;
                }
            }
            if(ring_radius>0.0f){
                float dx=px-ring_x;
                float dy=py-ring_y;
                float d=SDL_fabsf(SDL_sqrtf(dx*dx+dy*dy)-ring_radius);
                if(d<distance){
                    distance=d;
                }
            }

            float coverage=half_width+0.5f-distance;
            if(coverage<=0.0f){
                continue;
            }
            if(coverage>1.0f){
                coverage=1.0f;
            }
            SDL_WriteSurfacePixel(surface,x,y,255,255,255,
                (Uint8)SDL_roundf(coverage*255.0f));
        }
    }
}

/* 生成内置图标：× 或放大镜。纹理按物理像素生成，绘制时缩放到逻辑尺寸，
 * 这样高分屏上不需要依赖字体字形或主题资源里的图片。 */
static SDL_Texture *input_icon_create(int kind,float size){
    SDL_Renderer *r=get_renderer();
    if(r==NULL||size<=0.0f){
        return NULL;
    }

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    int pixels=(int)SDL_roundf(size*scale);
    if(pixels<4){
        pixels=4;
    }

    SDL_Surface *surface=SDL_CreateSurface(pixels,pixels,SDL_PIXELFORMAT_RGBA32);
    if(surface==NULL){
        return NULL;
    }
    SDL_ClearSurface(surface,0.0f,0.0f,0.0f,0.0f);

    float extent=(float)pixels;
    float half=SDL_max(1.0f,extent*0.07f);
    float inset=extent*0.26f;
    input_icon_stroke_t strokes[2];
    int count=0;
    float ring_x=0.0f;
    float ring_y=0.0f;
    float ring_radius=0.0f;

    if(kind==INPUT_ICON_SEARCH){
        ring_radius=extent*0.24f;
        ring_x=extent*0.42f;
        ring_y=extent*0.42f;
        float diagonal=ring_radius*0.7071f;
        strokes[0]=(input_icon_stroke_t){
            .ax=ring_x+diagonal,.ay=ring_y+diagonal,
            .bx=extent-inset,.by=extent-inset
        };
        count=1;
    }else{
        strokes[0]=(input_icon_stroke_t){
            .ax=inset,.ay=inset,.bx=extent-inset,.by=extent-inset
        };
        strokes[1]=(input_icon_stroke_t){
            .ax=extent-inset,.ay=inset,.bx=inset,.by=extent-inset
        };
        count=2;
    }

    input_icon_paint(surface,pixels,ring_x,ring_y,ring_radius,strokes,count,half);

    SDL_Texture *texture=SDL_CreateTextureFromSurface(r,surface);
    SDL_DestroySurface(surface);
    return texture;
}

static void input_widget_draw(void *p){
    widget_input_t *widget=p;
    if(widget==NULL||widget->box==NULL||get_renderer()==NULL){
        return;
    }

    draft_draw_rrect(widget->box);

    draft_texture_t *icon=widget->icon;
    if(icon==NULL||icon->tex==NULL){
        return;
    }

    icon->x=widget->l.x+widget->padding;
    icon->y=widget->l.y+(widget->l.height-icon->height)/2.0f;
    if(widget->icon_owned){
        unsigned color=widget->icon_color;
        SDL_SetTextureColorMod(icon->tex,
            (Uint8)((color>>24)&0xFF),
            (Uint8)((color>>16)&0xFF),
            (Uint8)((color>>8)&0xFF));
        SDL_SetTextureAlphaMod(icon->tex,(Uint8)(color&0xFF));
    }
    draft_draw_texture(icon);
}

/* 占位文字放在独立子布局里，裁剪范围等于文字区，不会盖到图标或清除按钮上 */
static void input_placeholder_draw(void *p){
    layout_t *l=p;
    widget_input_t *widget=l!=NULL?(widget_input_t *)l->parent:NULL;
    if(widget==NULL||widget->placeholder==NULL||widget->input==NULL){
        return;
    }
    if(widget->input->length>0){
        return;
    }

    draft_text_t *text=widget->placeholder;
    text->x=l->x;
    text->y=l->y+(l->height-text->height)/2.0f;
    draft_draw_text(text);
}

/* 光标与清除按钮画在文字之后，保证不会被文字盖住 */
static void input_overlay_draw(void *p){
    layout_t *l=p;
    widget_input_t *widget=l!=NULL?(widget_input_t *)l->parent:NULL;
    if(widget==NULL||widget->input==NULL||get_renderer()==NULL){
        return;
    }

    layout_input_t *input=widget->input;
    float center_x=l->x+l->width-widget->padding-widget->clear_size/2.0f;
    float center_y=l->y+l->height/2.0f;

    if(widget->clear_visible&&widget->clear_icon!=NULL&&widget->clear_icon->tex!=NULL){
        float diameter=widget->clear_size+8.0f;
        if(widget->clear_hover&&widget->clear_bg!=NULL){
            widget->clear_bg->x=center_x-diameter/2.0f;
            widget->clear_bg->y=center_y-diameter/2.0f;
            widget->clear_bg->width=diameter;
            widget->clear_bg->height=diameter;
            widget->clear_bg->r=diameter/2.0f;
            widget->clear_bg->auto_radius=0;
            widget->clear_bg->color=(widget->clear_color&0xFFFFFF00u)|0x22u;
            widget->clear_bg->border_color=0;
            draft_draw_rrect(widget->clear_bg);
        }

        SDL_Texture *texture=widget->clear_icon->tex;
        unsigned color=widget->clear_color;
        Uint8 alpha=(Uint8)(color&0xFF);
        if(!widget->clear_hover){
            alpha=(Uint8)((unsigned)alpha*3u/5u);
        }
        SDL_SetTextureColorMod(texture,
            (Uint8)((color>>24)&0xFF),
            (Uint8)((color>>16)&0xFF),
            (Uint8)((color>>8)&0xFF));
        SDL_SetTextureAlphaMod(texture,alpha);
        widget->clear_icon->x=center_x-widget->clear_icon->width/2.0f;
        widget->clear_icon->y=center_y-widget->clear_icon->height/2.0f;
        draft_draw_texture(widget->clear_icon);
    }

    if(!widget->caret_visible||!layout_input_is_focused(input)){
        return;
    }

    float caret_x=input->l.x+layout_input_prefix_width(input)-input->text_scroll;
    /* 光标覆盖完整字体行高，再略加一点高度，避免看起来比文字矮一截。 */
    float height=widget->line_height+widget->caret_inset;
    if(height<2.0f){
        height=2.0f;
    }
    if(height>input->l.height){
        height=input->l.height;
    }

    float right=input->l.x+input->l.width-widget->caret_width;
    if(caret_x<input->l.x){
        caret_x=input->l.x;
    }else if(caret_x>right){
        caret_x=right;
    }

    SDL_FRect caret={
        .x=caret_x,
        .y=input->l.y+(input->l.height-height)/2.0f,
        .w=widget->caret_width,
        .h=height
    };
    input_widget_draw_color(widget->caret_color);
    SDL_RenderFillRect(get_renderer(),&caret);
}

static void input_widget_update(void *p){
    widget_input_t *widget=p;
    if(widget==NULL||widget->input==NULL){
        return;
    }

    layout_update(&widget->l);

    layout_input_t *input=widget->input;
    float width=widget->l.width;
    float height=widget->l.height;
    if(width<=0.0f||height<=0.0f){
        return;
    }

    int hover=input_widget_contains(&widget->l,get_mouse_x(),get_mouse_y());
    int focused=input->focused?1:0;

    /* 键盘/粘贴在 sdlgui.c 里直接改 layout_input；这里统一检测并转发 change 回调。 */
    const char *text=layout_input_text(input);
    int text_changed=input->length!=widget->last_length||
        (widget->last_text!=NULL?strcmp(text,widget->last_text)!=0:text[0]!='\0');
    int cursor_changed=input->cursor!=widget->last_cursor;

    if(text_changed&&widget->change_callback!=NULL){
        widget->change_callback(text,widget->change_data);
    }

    if(text_changed||cursor_changed){
        if(focused){
            layout_input_scroll_into_view(input);
        }
        widget->caret_time=SDL_GetTicks();
    }else if(focused&&!widget->last_focused){
        widget->caret_time=SDL_GetTicks();
    }
    widget->last_focused=focused?1u:0u;

    /* 回调里可能改写了文字，重新取一次再同步基准值 */
    text=layout_input_text(input);
    if(widget->last_text==NULL||strcmp(text,widget->last_text)!=0){
        char *copy=input_widget_strdup(text);
        if(copy!=NULL){
            free(widget->last_text);
            widget->last_text=copy;
        }
    }
    widget->last_length=input->length;
    widget->last_cursor=input->cursor;

    /* 几何：左图标 + 文字区 + 右清除按钮 */
    float icon_width=0.0f;
    if(widget->icon!=NULL){
        icon_width=widget->icon->width+widget->icon_gap;
    }
    int clear_visible=widget->show_clear&&input->length>0&&(focused||hover);
    widget->clear_visible=clear_visible?1u:0u;
    float clear_width=clear_visible?widget->clear_size+widget->clear_gap:0.0f;

    float text_left=widget->l.x+widget->padding+icon_width;
    float text_width=widget->l.x+width-widget->padding-clear_width-text_left;
    if(text_width<1.0f){
        text_width=1.0f;
    }

    input->l.x=text_left;
    input->l.y=widget->l.y;
    input->l.width=text_width;
    input->l.height=height;

    if(widget->placeholder_box!=NULL){
        widget->placeholder_box->x=text_left;
        widget->placeholder_box->y=widget->l.y;
        widget->placeholder_box->width=text_width;
        widget->placeholder_box->height=height;
    }
    if(widget->overlay!=NULL){
        widget->overlay->x=widget->l.x;
        widget->overlay->y=widget->l.y;
        widget->overlay->width=width;
        widget->overlay->height=height;
    }

    if(widget->box!=NULL){
        widget->box->x=widget->l.x;
        widget->box->y=widget->l.y;
        widget->box->width=width;
        widget->box->height=height;
        widget->box->r=widget->corner_radius;
        widget->box->auto_radius=0;
    }

    /* 背景/边框朝目标色渐变，避免聚焦时颜色突跳 */
    unsigned back_color=focused?widget->focus_color:(hover?widget->hover_color:widget->color);
    unsigned border_color=focused?widget->focus_border_color:widget->border_color;
    unsigned long now=SDL_GetTicks();
    if(back_color!=widget->anim_to_color||border_color!=widget->anim_to_border){
        widget->anim_from_color=widget->shown_color;
        widget->anim_from_border=widget->shown_border;
        widget->anim_to_color=back_color;
        widget->anim_to_border=border_color;
        widget->anim_start=now;
    }
    unsigned long elapsed=now-widget->anim_start;
    widget->shown_color=input_color_gradient(widget->anim_to_color,widget->anim_from_color,
        elapsed,widget->animation_time);
    widget->shown_border=input_color_gradient(widget->anim_to_border,widget->anim_from_border,
        elapsed,widget->animation_time);
    if(widget->box!=NULL){
        widget->box->color=widget->shown_color;
        widget->box->border_color=widget->shown_border;
    }

    /* 光标闪烁：编辑后先常亮一个周期，之后按半周期亮灭 */
    int caret_visible=0;
    if(focused){
        unsigned long half=widget->caret_blink_time/2;
        unsigned long idle=now-widget->caret_time;
        if(widget->caret_blink_time==0||half==0||idle<widget->caret_blink_time){
            caret_visible=1;
        }else{
            caret_visible=((idle/half)%2)==0?1:0;
        }
    }
    widget->caret_visible=caret_visible?1u:0u;
}

static void input_widget_mouse(void *p){
    widget_input_t *widget=p;
    if(widget==NULL||widget->input==NULL){
        return;
    }

    layout_t *l=&widget->l;
    int pressed=get_mouse_left()?1:0;
    int inside=input_widget_contains(l,get_mouse_x(),get_mouse_y());

    float center_x=l->x+l->width-widget->padding-widget->clear_size/2.0f;
    float center_y=l->y+l->height/2.0f;
    float hit=widget->clear_size/2.0f+4.0f;
    int clear_hit=0;
    if(widget->clear_visible){
        float mouse_x=get_mouse_x();
        float mouse_y=get_mouse_y();
        clear_hit=mouse_x>=center_x-hit&&mouse_x<=center_x+hit&&
                 mouse_y>=center_y-hit&&mouse_y<=center_y+hit;
    }
    widget->clear_hover=(inside&&clear_hit)?1u:0u;

    if(!widget->controllable){
        widget->last_left=pressed?1u:0u;
        return;
    }

    if(pressed&&!widget->last_left&&clear_hit){
        widget->clear_press=1;
    }
    if(!pressed&&widget->last_left&&widget->clear_press){
        widget->clear_press=0;
        if(clear_hit){
            layout_input_focus(widget->input);
            layout_input_set_text(widget->input,"");
            layout_input_set_cursor(widget->input,0);
        }
    }

    /* 文字区按下或按住拖动：光标贴到离鼠标最近的字符边界 */
    if(pressed&&inside&&!clear_hit){
        layout_input_focus(widget->input);
        layout_input_set_cursor_at(widget->input,get_mouse_x());
        widget->caret_time=SDL_GetTicks();
    }

    widget->last_left=pressed?1u:0u;
}

static void input_widget_clear(void *p){
    widget_input_t *widget=p;
    if(widget==NULL){
        return;
    }

    /* 子布局已经先于本函数被 delete_layout() 删除，这里只清自己的资源 */
    widget->input=NULL;
    widget->placeholder_box=NULL;
    widget->overlay=NULL;

    if(widget->placeholder!=NULL){
        draft_text_clear(widget->placeholder);
        free(widget->placeholder);
        widget->placeholder=NULL;
    }
    free(widget->placeholder_text);
    widget->placeholder_text=NULL;

    if(widget->icon!=NULL){
        draft_texture_clear(widget->icon);   /* 只销毁自己创建的纹理 */
        free(widget->icon);
        widget->icon=NULL;
    }
    if(widget->clear_icon!=NULL){
        draft_texture_clear(widget->clear_icon);
        free(widget->clear_icon);
        widget->clear_icon=NULL;
    }

    free(widget->clear_bg);
    widget->clear_bg=NULL;
    free(widget->box);
    widget->box=NULL;
    free(widget->last_text);
    widget->last_text=NULL;
}

void widget_input_set_placeholder(widget_input_t *widget,const char *placeholder){
    if(widget==NULL||widget->input==NULL){
        return;
    }

    if(widget->placeholder!=NULL){
        draft_text_clear(widget->placeholder);
        free(widget->placeholder);
        widget->placeholder=NULL;
    }
    free(widget->placeholder_text);
    widget->placeholder_text=NULL;

    if(placeholder==NULL||placeholder[0]=='\0'){
        return;
    }

    char *copy=input_widget_strdup(placeholder);
    if(copy==NULL){
        return;
    }

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    draft_text_t *text=draft_create_text(zero,widget->input->font,copy,0.0f,0.0f,
        widget->placeholder_color);
    if(text==NULL){
        free(copy);
        return;
    }
    text->can_enter=0;
    text->can_change=0;
    widget->placeholder_text=copy;
    widget->placeholder=text;
}

void widget_input_set_icon(widget_input_t *widget,SDL_Texture *icon){
    if(widget==NULL){
        return;
    }

    if(widget->icon!=NULL){
        draft_texture_clear(widget->icon);
        free(widget->icon);
        widget->icon=NULL;
    }
    widget->icon_owned=0;
    if(icon==NULL){
        return;
    }

    float size=widget->icon_size>0.0f?widget->icon_size:16.0f;
    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    widget->icon=draft_create_texture(zero,icon,size,size);
}

void widget_input_set_clear(widget_input_t *widget,int enable){
    if(widget==NULL){
        return;
    }

    widget->show_clear=enable?1u:0u;
    if(!widget->show_clear||widget->clear_icon!=NULL){
        return;
    }

    float size=widget->clear_size>0.0f?widget->clear_size:16.0f;
    SDL_Texture *texture=input_icon_create(INPUT_ICON_CROSS,size);
    if(texture==NULL){
        widget->show_clear=0;
        return;
    }

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    draft_texture_t *draft=draft_create_texture(zero,texture,size,size);
    if(draft==NULL){
        SDL_DestroyTexture(texture);
        widget->show_clear=0;
        return;
    }
    draft->owns_texture=1;
    widget->clear_icon=draft;
}

void widget_input_set_text(widget_input_t *widget,const char *text){
    if(widget==NULL||widget->input==NULL){
        return;
    }

    layout_input_set_text(widget->input,text);
    /* 程序化改写文本后光标放到末尾，用户可以直接接着输入 */
    layout_input_set_cursor(widget->input,widget->input->length);
    widget->caret_time=SDL_GetTicks();
}

const char *widget_input_get_text(const widget_input_t *widget){
    if(widget==NULL||widget->input==NULL){
        return "";
    }
    return layout_input_text(widget->input);
}

void widget_input_set_max_bytes(widget_input_t *widget,size_t max_bytes){
    if(widget==NULL||widget->input==NULL){
        return;
    }

    widget->input->max_bytes=max_bytes;
    if(max_bytes>0&&widget->input->length>max_bytes){
        /* 复用 set_text 的截断逻辑，保证不会切出半个 UTF-8 字符 */
        layout_input_set_text(widget->input,widget->input->text);
    }
}

void widget_input_focus(widget_input_t *widget){
    if(widget!=NULL&&widget->input!=NULL){
        layout_input_focus(widget->input);
        widget->caret_time=SDL_GetTicks();
    }
}

void widget_input_blur(widget_input_t *widget){
    if(widget!=NULL&&widget->input!=NULL&&layout_input_is_focused(widget->input)){
        layout_input_blur();
    }
}

int widget_input_is_focused(const widget_input_t *widget){
    return widget!=NULL&&layout_input_is_focused(widget->input);
}

void widget_input_set_caret_blink(widget_input_t *widget,unsigned long blink_time){
    if(widget==NULL){
        return;
    }
    widget->caret_blink_time=blink_time;
    widget->caret_time=SDL_GetTicks();
    widget->caret_visible=1;
}

/* 控件创建的图标纹理归控件所有（清除按钮、放大镜），clear 时随 draft 一起销毁 */
static int input_widget_take_icon(widget_input_t *widget,SDL_Texture *texture,float size){
    if(widget==NULL||texture==NULL){
        if(texture!=NULL){
            SDL_DestroyTexture(texture);
        }
        return 0;
    }

    if(widget->icon!=NULL){
        draft_texture_clear(widget->icon);
        free(widget->icon);
        widget->icon=NULL;
    }

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    draft_texture_t *draft=draft_create_texture(zero,texture,size,size);
    if(draft==NULL){
        SDL_DestroyTexture(texture);
        widget->icon_owned=0;
        return 0;
    }

    draft->owns_texture=1;
    widget->icon=draft;
    widget->icon_owned=1;
    widget->icon_size=size;
    return 1;
}

/* 回车从 layout_input 转发到控件的 submit_callback */
static void input_widget_submit(const char *text,void *data){
    widget_input_t *widget=data;
    if(widget!=NULL&&widget->submit_callback!=NULL){
        widget->submit_callback(text,widget->submit_data);
    }
}

widget_input_t *widget_create_raw_input(void *parent,float w,float h,float r,align_t align,
    TTF_Font *font,const char *text,
    unsigned color,unsigned hover_color,unsigned focus_color,
    unsigned border_color,unsigned focus_border_color,
    unsigned text_color,unsigned placeholder_color,unsigned caret_color,unsigned clear_color,
    unsigned long animation_time,unsigned long caret_blink_time,
    input_change_func change_callback,void *change_data,
    input_callback_func submit_callback,void *submit_data){
    if(w<=0.0f||h<=0.0f||font==NULL){
        return NULL;
    }

    widget_input_t *widget=(widget_input_t *)create_layout(
        sizeof(*widget),
        parent,
        w,
        h,
        input_widget_draw,
        input_widget_update,
        input_widget_clear,
        input_widget_mouse,
        align
    );
    if(widget==NULL){
        return NULL;
    }

    widget->color=color;
    widget->hover_color=hover_color;
    widget->focus_color=focus_color;
    widget->border_color=border_color;
    widget->focus_border_color=focus_border_color;
    widget->text_color=text_color;
    widget->placeholder_color=placeholder_color;
    widget->caret_color=caret_color;
    widget->clear_color=clear_color;
    widget->icon_color=text_color;

    widget->corner_radius=r>=0.0f?r:h/2.0f;
    widget->padding=SDL_max(8.0f,h*0.3f);
    widget->icon_size=SDL_min(h*0.55f,18.0f);
    widget->icon_gap=SDL_max(4.0f,h*0.2f);
    widget->clear_size=SDL_min(h*0.5f,16.0f);
    widget->clear_gap=SDL_max(4.0f,h*0.15f);
    widget->caret_width=SDL_max(1.0f,SDL_floorf(h/16.0f));
    widget->caret_inset=SDL_max(2.0f,h*0.12f);
    widget->animation_time=animation_time;
    widget->caret_blink_time=caret_blink_time;

    float scale=sdlgui_get_scale();
    if(scale<=0.0f){
        scale=1.0f;
    }
    widget->line_height=(float)TTF_GetFontHeight(font)/scale;
    if(widget->line_height<=0.0f){
        widget->line_height=h;
    }

    widget->shown_color=color;
    widget->shown_border=border_color;
    widget->anim_from_color=color;
    widget->anim_from_border=border_color;
    widget->anim_to_color=color;
    widget->anim_to_border=border_color;
    widget->anim_start=SDL_GetTicks();
    widget->caret_time=widget->anim_start;
    widget->controllable=1;
    widget->change_callback=change_callback;
    widget->change_data=change_data;
    widget->submit_callback=submit_callback;
    widget->submit_data=submit_data;

    widget->last_text=input_widget_strdup(text!=NULL?text:"");
    if(widget->last_text==NULL){
        delete_layout((layout_t *)widget);
        return NULL;
    }
    widget->last_length=strlen(widget->last_text);
    widget->last_cursor=widget->last_length;

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);

    widget->box=draft_create_rrect(zero,w,h,widget->corner_radius,color,border_color);
    widget->clear_bg=draft_create_rrect(zero,1.0f,1.0f,1.0f,0,0);
    widget->placeholder_box=create_layout(sizeof(layout_t),(layout_t *)widget,
        1.0f,1.0f,input_placeholder_draw,layout_update,NULL,NULL,zero);
    widget->input=create_layout_input(widget,1.0f,1.0f,zero,text,font,text_color,0,
        input_widget_submit,widget);
    widget->overlay=create_layout(sizeof(layout_t),(layout_t *)widget,
        1.0f,1.0f,input_overlay_draw,layout_update,NULL,NULL,zero);
    if(widget->box==NULL||widget->clear_bg==NULL||
       widget->placeholder_box==NULL||widget->input==NULL||widget->overlay==NULL){
        delete_layout((layout_t *)widget);
        return NULL;
    }

    input_widget_update(widget);
    return widget;
}

widget_input_t *widget_create_input(void *parent,float width,align_t align,
    const char *placeholder,
    input_change_func change_callback,void *change_data,
    input_callback_func submit_callback,void *submit_data){
    if(width<=0.0f){
        layout_t *p=parent!=NULL?(layout_t *)parent:get_root_layout();
        width=p!=NULL?p->width:0.0f;
    }
    if(width<=0.0f){
        width=200.0f;
    }

    float height=input_theme_height();
    TTF_Font *font=obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT));
    if(font==NULL){
        return NULL;
    }

    /* 主题没有专门的输入框色，用面板色做底、聚焦时退到主色，边框用分割线色。
     * 悬停色取面板色到按钮色之间的 1/4，保持主题配色统一。 */
    unsigned text_color=get_text_color(TEXT_CONTEXT);
    unsigned placeholder_color=(text_color&0xFFFFFF00u)|0x80u;
    unsigned hover_color=input_color_gradient(get_color(COLOR_BUTTON),
        get_color(COLOR_SECONDARY),1,4);

    widget_input_t *widget=widget_create_raw_input(
        parent,
        width,
        height,
        get_theme()->rect_round_corner,
        align,
        font,
        "",
        get_color(COLOR_SECONDARY),
        hover_color,
        get_color(COLOR_MAIN),
        get_divider_color(),
        get_color(COLOR_BUTTON_HIGHLIGHT),
        text_color,
        placeholder_color,
        text_color,
        placeholder_color,
        get_theme()->button_animation_time,
        INPUT_DEFAULT_BLINK,
        change_callback,
        change_data,
        submit_callback,
        submit_data
    );
    if(widget==NULL){
        return NULL;
    }

    widget_input_set_placeholder(widget,placeholder);
    return widget;
}

widget_input_t *widget_create_search_input(void *parent,float width,align_t align,
    const char *placeholder,
    input_change_func change_callback,void *change_data,
    input_callback_func submit_callback,void *submit_data){
    widget_input_t *widget=widget_create_input(parent,width,align,placeholder,
        change_callback,change_data,submit_callback,submit_data);
    if(widget==NULL){
        return NULL;
    }

    float size=widget->icon_size>0.0f?widget->icon_size:16.0f;
    SDL_Texture *icon=input_icon_create(INPUT_ICON_SEARCH,size);
    if(icon!=NULL&&input_widget_take_icon(widget,icon,size)){
        widget->icon_color=widget->placeholder_color;
    }
    widget_input_set_clear(widget,1);

    input_widget_update(widget);
    return widget;
}


void layout_input_root_mouse(void){
    int left=get_mouse_left();
    if(left&&!input_last_left&&focused_input!=NULL&&
       !input_contains_mouse(&focused_input->l)){
        layout_input_blur();
    }
    input_last_left=left;
}

void input_quit(void){
    if(input_text_cursor!=NULL){
        SDL_SetCursor(SDL_GetDefaultCursor());
        SDL_DestroyCursor(input_text_cursor);
        input_text_cursor=NULL;
    }
    focused_input=NULL;
    input_hover=NULL;
    input_last_left=0;
}
