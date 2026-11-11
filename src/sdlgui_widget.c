#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "sdlgui_widget.h"
#include "CDS/darray.h"
#include <stdlib.h>


#define MAX(a,b) ((a)>(b)?(a):(b))

/* 图标+文字控件里，图标高度相对控件高度的比例 */
#define ICON_TEXT_SCALE 0.8f

/* timer 是已过去的时间，whole 是总时长；返回 origin -> target 的插值颜色 */
static unsigned color_gradient(unsigned target,unsigned origin,size_t timer,size_t whole){
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

typedef struct {
    unsigned origin_color;   /* 本次动画起始颜色 */
    unsigned target_color;   /* 本次动画目标颜色 */
    unsigned base_color;     /* 常态背景色 */
    unsigned hover_color;    /* 悬停背景色 */
    unsigned press_color;    /* 按下背景色 */
    unsigned long duration;  /* 颜色动画时长（ms） */
    int trigger_hold;        /* 触发色是否还在展示 */
    int pending_status;      /* 触发色结束后要切换到的状态 */
}button_anim_data_t;

static unsigned button_status_color(const button_anim_data_t *data,int status){
    if(status==BUTTON_STATUS_HOVER){
        return data->hover_color;
    }
    if(status==BUTTON_STATUS_TRIGGER){
        return data->press_color;
    }
    return data->base_color;
}

/* 动画数据挂在 layout_animation_t.data 上，由动画自身的 clear 释放。 */
static void button_animation_clear(void *p){
    layout_animation_t *animation=p;
    if(animation==NULL){
        return;
    }
    free(animation->data);
    animation->data=NULL;
    animation_clear(p);
}

/* 本次颜色动画结束：如果触发色还在保持，等它结束后再切到待处理状态。 */
static void button_animation_finish(button_anim_data_t *data,layout_animation_t *animation,draft_rrect_t *draft){
    animation->lasting=0;
    animation->status=0;

    if(data->trigger_hold){
        data->trigger_hold=0;
        int pending=data->pending_status;
        if(pending!=BUTTON_STATUS_TRIGGER){
            data->origin_color=draft->color;
            data->target_color=button_status_color(data,pending);
            animation->status=pending;
            animation->timer=SDL_GetTicks();
            animation->lasting=1;
        }
    }
}

static void button_animation(void *a){
    layout_animation_t *animation=a;
    if(animation==NULL||animation->drafts==NULL||animation->draft_size==0){
        return;
    }

    draft_rrect_t *draft=(draft_rrect_t *)animation->drafts[0];
    button_anim_data_t *data=animation->data;
    if(draft==NULL||data==NULL){
        return;
    }

    unsigned target=data->target_color;
    unsigned long duration=data->duration;
    if(animation->lasting&&duration>0){
        unsigned long elapsed=SDL_GetTicks()-animation->timer;
        if(elapsed<duration){
            unsigned color=color_gradient(target,data->origin_color,elapsed,duration);
            draft->color=color;
            draft->border_color=color;
            return;
        }
    }

    draft->color=target;
    draft->border_color=target;
    button_animation_finish(data,animation,draft);
}

static layout_animation_t *widget_find_animation(void *p){
    layout_t *parent=p;
    if(parent==NULL){
        return NULL;
    }
    for(size_t i=0;i<parent->son_size;i++){
        if(parent->sons[i]!=NULL&&parent->sons[i]->draw==animation_draw){
            return (layout_animation_t *)parent->sons[i];
        }
    }
    return NULL;
}

static void button_callback(int status,void *data){
    widget_button_t *widget=data;
    if(widget==NULL){
        return;
    }

    layout_animation_t *animation=widget_find_animation(&widget->l);
    if(animation==NULL||animation->drafts==NULL||animation->draft_size==0){
        return;
    }
    button_anim_data_t *anim=animation->data;
    draft_rrect_t *draft=(draft_rrect_t *)animation->drafts[0];
    if(anim==NULL||draft==NULL){
        return;
    }

    /* 触发色展示期间，鼠标移入/移出只记录待处理状态，不立刻覆盖确认色。 */
    if(status!=BUTTON_STATUS_TRIGGER&&anim->trigger_hold){
        anim->pending_status=status;
        return;
    }

    anim->origin_color=draft->color;
    anim->target_color=button_status_color(anim,status);
    if(status==BUTTON_STATUS_TRIGGER){
        anim->trigger_hold=1;
        anim->pending_status=BUTTON_STATUS_TRIGGER;
    }

    animation->status=status;
    animation->timer=SDL_GetTicks();
    animation->lasting=1;
    if(animation->animation!=NULL){
        animation->animation(animation);
    }

    if(status==BUTTON_STATUS_TRIGGER&&widget->callback!=NULL){
        widget->callback(status,widget->data);
    }
}

widget_button_t *widget_create_raw_button(void *parent,float w,float h,float r,align_t align,
    TTF_Font *font,const char *text,
    unsigned color,unsigned hover_color,unsigned press_color,unsigned text_color,
    unsigned long animation_time,
    button_callback_func callback,void *data){
    if(w<=0.0f||h<=0.0f||font==NULL){
        return NULL;
    }

    widget_button_t *widget=create_layout(
        sizeof(*widget),
        parent,
        w,
        h,
        NULL,
        layout_update,
        NULL,
        NULL,
        align
    );
    if(widget==NULL){
        return NULL;
    }

    widget->callback=callback;
    widget->data=data;

    align_t full=make_align(3,3,1,1,0,0,1.0f,1.0f);

    const char *context=text!=NULL?text:"";
    layout_animation_t *animation=create_layout_animation(widget,button_animation,
        draft_create_rrect(full,w,h,r,color,color),
        draft_create_icon_with_text(
            make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
            font,
            (char *)context,
            w,
            h*ICON_TEXT_SCALE,
            text_color
        ),
        NULL
    );
    if(animation==NULL){
        delete_layout((layout_t *)widget);
        return NULL;
    }

    button_anim_data_t *anim=calloc(1,sizeof(*anim));
    if(anim==NULL){
        delete_layout((layout_t *)widget);
        return NULL;
    }
    anim->origin_color=color;
    anim->target_color=color;
    anim->base_color=color;
    anim->hover_color=hover_color;
    anim->press_color=press_color;
    anim->duration=animation_time;
    anim->pending_status=BUTTON_STATUS_OUT;
    animation->data=anim;
    animation->l.clear=button_animation_clear;

    bbox_rrect_t *bbox=bbox_create_rrect(full,w,h,r);
    layout_button_t *button=create_layout_button(widget,w,h,full,button_callback,widget,bbox,NULL);
    (void)button;

    return widget;
}

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,
    button_callback_func callback,void *data){
    float height=get_theme()->button_height;
    if(get_theme()->item_height>0.0f&&height>get_theme()->item_height){
        height=get_theme()->item_height;
    }
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    TTF_Font *font=obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT));
    if(font==NULL){
        return NULL;
    }

    return widget_create_raw_button(
        parent,
        width,
        height,
        get_theme()->rect_round_corner,
        align,
        font,
        text,
        get_color(COLOR_BUTTON),
        get_color(COLOR_BUTTON_HIGHLIGHT),
        get_color(COLOR_BUTTON_CONFIRM),
        get_text_color(TEXT_CONTEXT),
        get_theme()->button_animation_time,
        callback,
        data
    );
}

typedef struct {
    draft_rrect_t *track;
    draft_rrect_t *on_block;
    draft_texture_t *knob;
}switch_drafts_t;

static int switch_get_drafts(layout_animation_t *animation,switch_drafts_t *d){
    if(animation==NULL||animation->drafts==NULL||animation->draft_size<3){
        return 0;
    }
    d->track=(draft_rrect_t *)animation->drafts[0];
    d->on_block=(draft_rrect_t *)animation->drafts[1];
    d->knob=(draft_texture_t *)animation->drafts[2];
    return d->track!=NULL&&d->on_block!=NULL;
}

/* 按当前旋钮位置更新轨道、开启色块和旋钮。 */
static void switch_apply(switch_drafts_t *d,float left,float top,float width,float height,
    float knob_x,int on,unsigned off_color,unsigned on_color,float corner_radius){
    draft_rrect_t *track=d->track;
    draft_rrect_t *on_block=d->on_block;
    float track_w=track->width;
    float track_h=track->height;
    if(track_w<=0.0f||track_h<=0.0f){
        return;
    }

    float track_x=left+(width-track_w)/2.0f;
    float track_y=top+(height-track_h)/2.0f;
    track->x=track_x;
    track->y=track_y;
    track->r=corner_radius;
    track->auto_radius=0;
    track->color=off_color;
    track->border_color=0;

    float knob_w=d->knob!=NULL?d->knob->width:0.0f;
    float travel=width-knob_w;
    float progress=0.0f;
    if(d->knob!=NULL&&travel>0.0f){
        progress=(knob_x-left)/travel;
    }else{
        progress=on?1.0f:0.0f;
    }
    if(progress<0.0f){
        progress=0.0f;
    }else if(progress>1.0f){
        progress=1.0f;
    }

    on_block->x=track_x;
    on_block->y=track_y;
    on_block->width=track_w*progress;
    on_block->height=track_h;
    on_block->r=corner_radius;
    on_block->auto_radius=0;
    on_block->color=on_color;
    on_block->border_color=0;

    if(d->knob!=NULL){
        d->knob->x=knob_x;
    }
}

static void switch_update(void *p){
    widget_switch_t *sw=p;
    if(sw==NULL){
        return;
    }

    layout_update(&sw->l);
    layout_animation_t *animation=widget_find_animation(sw);
    switch_drafts_t d;
    if(!switch_get_drafts(animation,&d)){
        return;
    }

    float width=sw->l.width;
    float knob_w=d.knob!=NULL?d.knob->width:0.0f;
    float left=sw->l.x;
    float right=left+width-knob_w;
    if(right<left){
        right=left;
    }

    /* 静止时由布局更新保证旋钮贴住当前状态；动画期间由 animation 推进。 */
    if(animation->status==0&&d.knob!=NULL){
        d.knob->x=sw->on?right:left;
    }

    switch_apply(&d,left,sw->l.y,width,sw->l.height,
        d.knob!=NULL?d.knob->x:left,sw->on,sw->off_color,sw->on_color,
        sw->corner_radius);
}

static void switch_animation(void *a){
    layout_animation_t *animation=a;
    if(animation==NULL){
        return;
    }
    widget_switch_t *sw=(widget_switch_t *)animation->l.parent;
    if(sw==NULL){
        return;
    }

    switch_drafts_t d;
    if(!switch_get_drafts(animation,&d)){
        return;
    }
    if(d.knob==NULL){
        animation->status=0;
        animation->lasting=0;
        return;
    }

    float left=animation->l.x;
    float right=left+animation->l.width-d.knob->width;
    if(right<left){
        right=left;
    }

    unsigned long now=SDL_GetTicks();
    unsigned long elapsed=now-animation->timer;
    animation->timer=now;

    float need=(float)sw->animation_time;
    float step=need>0.0f?(right-left)*((float)elapsed/need):(right-left);

    if(animation->status==1){
        d.knob->x+=step;
        if(d.knob->x>=right){
            d.knob->x=right;
            animation->status=0;
            animation->lasting=0;
        }
    }else if(animation->status==2){
        d.knob->x-=step;
        if(d.knob->x<=left){
            d.knob->x=left;
            animation->status=0;
            animation->lasting=0;
        }
    }

    int on=sw->on;
    switch_apply(&d,left,animation->l.y,animation->l.width,animation->l.height,
        d.knob->x,on,sw->off_color,sw->on_color,sw->corner_radius);
}
/* layout_button 只上报按钮状态，这里翻译成开关切换 */
static void switch_button_callback(int status,void *data){
    widget_switch_t *sw=data;
    if(sw==NULL||!sw->controllable||status!=BUTTON_STATUS_TRIGGER){
        return;
    }

    layout_animation_t *animation=widget_find_animation(sw);
    if(animation==NULL){
        return;
    }

    sw->on=sw->on?0:1;
    animation->timer=SDL_GetTicks();
    animation->status=sw->on?1:2;
    animation->lasting=1;
    switch_update(sw);

    if(sw->callback!=NULL){
        sw->callback(sw->on?1:0,sw->data);
    }
}

widget_switch_t *widget_create_raw_switch(void *parent,float w,float track_h,float knob_h,
    float r,align_t align,unsigned off_color,unsigned on_color,
    unsigned long animation_time,SDL_Texture *knob_icon,
    switch_callback_func callback,void *data){
    if(w<=0.0f||track_h<=0.0f||knob_h<=0.0f||knob_icon==NULL){
        return NULL;
    }

    float corner_radius=r>=0.0f?r:track_h/2.0f;
    float height=MAX(track_h,knob_h);
    float track_r=track_h/2.0f;
    float knob_r=knob_h/2.0f;
    float track_w=knob_r>track_r?w-knob_r*2.0f+track_r*2.0f:w;
    if(track_w<=0.0f){
        track_w=w;
    }

    widget_switch_t *sw=create_layout(
        sizeof(*sw),
        parent,
        w,
        height,
        NULL,
        switch_update,
        NULL,
        NULL,
        align
    );
    if(sw==NULL){
        return NULL;
    }

    sw->on=0;
    sw->controllable=1;
    sw->off_color=off_color;
    sw->on_color=on_color;
    sw->corner_radius=corner_radius;
    sw->animation_time=animation_time;
    sw->callback=callback;
    sw->data=data;

    align_t full=make_align(3,3,1,1,0,0,1.0f,1.0f);
    bbox_rrect_t *bbox=bbox_create_rrect(full,w,height,corner_radius);
    if(bbox==NULL){
        delete_layout((layout_t *)sw);
        return NULL;
    }
    layout_button_t *button=create_layout_button(sw,w,track_h,full,switch_button_callback,sw,bbox,NULL);
    if(button==NULL){
        free(bbox);
        delete_layout((layout_t *)sw);
        return NULL;
    }

    align_t track_align=make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f);
    align_t on_align=make_align(0,3,0,0,0.0f,0.0f,0.0f,0.0f);
    align_t knob_align=make_align(0,3,0,0,0.0f,0.0f,0.0f,0.0f);
    layout_animation_t *animation=create_layout_animation(
        sw,switch_animation,
        draft_create_rrect(track_align,track_w,track_h,corner_radius,off_color,0),
        draft_create_rrect(on_align,0.0f,track_h,corner_radius,on_color,0),
        draft_create_texture(knob_align,knob_icon,0.0f,knob_h),
        NULL
    );
    if(animation==NULL){
        delete_layout((layout_t *)sw);
        return NULL;
    }
    animation->timer=SDL_GetTicks();

    switch_update(sw);
    return sw;
}

widget_switch_t *widget_create_switch(void *parent,align_t align,switch_callback_func callback,void *data){
    float track_h=get_theme()->button_height;
    float knob_h=get_theme()->switch_icon_height;
    if(knob_h<=0.0f){
        knob_h=track_h;
    }
    float width=get_theme()->switch_width;
    SDL_Texture *knob=get_icon(ICON_KNOB);
    if(width<=0.0f||track_h<=0.0f||knob_h<=0.0f||knob==NULL){
        return NULL;
    }

    return widget_create_raw_switch(
        parent,
        width,
        track_h,
        knob_h,
        track_h/2.0f,
        align,
        get_color(COLOR_SWITCH_OFF),
        get_color(COLOR_SWITCH_ON),
        get_theme()->switch_animation_time,
        knob,
        callback,
        data
    );
}


void widget_switch_set(widget_switch_t *sw,int on){
    if(sw==NULL){
        return;
    }

    int state=on?1:0;
    if((sw->on?1:0)==state){
        return;
    }

    sw->on=state;
    layout_animation_t *animation=widget_find_animation(sw);
    if(animation!=NULL){
        animation->timer=SDL_GetTicks();
        animation->status=sw->on?1:2;
        animation->lasting=1;
    }
    switch_update(sw);
}


typedef struct {
    layout_t l;
    draft_texture_t *text;
}selector_label_t;

static void selector_label_draw(void *p){
    selector_label_t *label=p;
    if(label!=NULL&&label->text!=NULL){
        draft_draw_texture(label->text);
    }
}

static void selector_label_clear(void *p){
    selector_label_t *label=p;
    if(label==NULL){
        return;
    }
    if(label->text!=NULL){
        draft_texture_clear(label->text);
        free(label->text);
        label->text=NULL;
    }
}

static void selector_update(void *p);

static char *selector_strdup(const char *s){
    if(s==NULL){
        return NULL;
    }
    size_t size=strlen(s)+1;
    char *copy=malloc(size);
    if(copy!=NULL){
        memcpy(copy,s,size);
    }
    return copy;
}

static void selector_clear(void *p){
    widget_selector_t *selector=p;
    if(selector==NULL){
        return;
    }
    if(selector->items!=NULL){
        for(size_t i=0;i<selector->item_count;i++){
            free(selector->items[i]);
        }
        free(selector->items);
        selector->items=NULL;
    }
    selector->item_count=0;
}

static selector_label_t *selector_label_at(widget_selector_t *selector,size_t index){
    if(selector==NULL||index>=selector->item_count){
        return NULL;
    }
    size_t seen=0;
    for(size_t i=0;i<selector->l.l.son_size;i++){
        layout_t *son=selector->l.l.sons[i];
        if(son==NULL||son->draw!=selector_label_draw){
            continue;
        }
        if(seen==index){
            return (selector_label_t *)son;
        }
        seen++;
    }
    return NULL;
}

static void selector_label_apply(widget_selector_t *selector,selector_label_t *label,size_t index){
    if(selector==NULL||label==NULL){
        return;
    }

    if(label->text!=NULL){
        draft_texture_clear(label->text);
        free(label->text);
        label->text=NULL;
    }

    const char *text=selector->items!=NULL?selector->items[index]:NULL;
    if(text==NULL){
        text="";
    }
    label->text=draft_create_icon_with_text(
        make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
        selector->font,
        (char *)text,
        selector->item_width,
        selector->item_height*ICON_TEXT_SCALE,
        selector->text_color
    );
    if(label->text!=NULL){
        label->text->parent=(sublayout_t *)&label->l;
    }
}

static void selector_animation(void *a){
    layout_animation_t *animation=a;
    widget_selector_t *selector=animation!=NULL?(widget_selector_t *)animation->l.parent:NULL;
    if(selector==NULL||animation->draft_size<2){
        return;
    }

    draft_rrect_t *thumb=(draft_rrect_t *)animation->drafts[1];
    if(thumb==NULL){
        animation->lasting=0;
        animation->status=0;
        return;
    }

    unsigned long duration=selector->animation_time;
    if(!animation->lasting||duration==0){
        thumb->x=selector->to_x;
        animation->lasting=0;
        animation->status=0;
        return;
    }

    unsigned long elapsed=SDL_GetTicks()-animation->timer;
    if(elapsed>=duration){
        thumb->x=selector->to_x;
        animation->lasting=0;
        animation->status=0;
        return;
    }

    float t=(float)elapsed/(float)duration;
    thumb->x=selector->from_x+(selector->to_x-selector->from_x)*t;
}

static void selector_update(void *p){
    widget_selector_t *selector=p;
    if(selector==NULL){
        return;
    }

    layout_update(&selector->l.l);

    layout_animation_t *animation=widget_find_animation(selector);
    if(animation==NULL||animation->draft_size<2){
        return;
    }
    layout_update(&animation->l);

    draft_rrect_t *track=(draft_rrect_t *)animation->drafts[0];
    draft_rrect_t *thumb=(draft_rrect_t *)animation->drafts[1];
    if(track==NULL||thumb==NULL){
        return;
    }

    float width=animation->l.width;
    float height=animation->l.height;
    float radius=selector->corner_radius;
    if(radius<0.0f){
        radius=height/2.0f;
    }

    track->x=animation->l.x;
    track->y=animation->l.y;
    track->width=width;
    track->height=height;
    track->r=radius;
    track->auto_radius=0;
    track->color=selector->track_color;
    track->border_color=0;

    thumb->y=animation->l.y;
    thumb->width=selector->item_width;
    thumb->height=height;
    thumb->r=radius;
    thumb->auto_radius=0;
    thumb->color=selector->selected_color;
    thumb->border_color=0;
    if(!animation->lasting){
        thumb->x=animation->l.x+selector->now_button*selector->item_width;
    }
}

static void selector_select(widget_selector_t *selector,size_t index){
    if(selector==NULL||index>=selector->item_count||index==selector->now_button){
        return;
    }

    size_t from=selector->now_button;
    layout_animation_t *animation=widget_find_animation(selector);
    if(animation!=NULL&&animation->draft_size>=2){
        selector_update(selector);
        draft_rrect_t *thumb=(draft_rrect_t *)animation->drafts[1];
        if(thumb!=NULL){
            selector->from_x=thumb->x;
            selector->to_x=animation->l.x+index*selector->item_width;
            selector->now_button=index;
            animation->timer=SDL_GetTicks();
            animation->lasting=1;
            animation->status=1;
            if(animation->animation!=NULL){
                animation->animation(animation);
            }
        }else{
            selector->now_button=index;
        }
    }else{
        selector->now_button=index;
    }

    if(selector->callback!=NULL){
        selector->callback(selector->now_button,from,selector->data);
    }
}

static void selector_mouse(void *p){
    widget_selector_t *selector=p;
    if(selector==NULL){
        return;
    }

    int pressed=get_mouse_left();
    if(selector->controllable&&pressed&&!selector->last_left){
        float mouse_x=get_mouse_x();
        float mouse_y=get_mouse_y();
        int inside=mouse_x>=selector->l.l.x&&mouse_x<=selector->l.l.x+selector->l.l.width
                && mouse_y>=selector->l.l.y&&mouse_y<=selector->l.l.y+selector->l.l.height;
        if(inside&&selector->item_width>0.0f){
            float local=mouse_x-selector->l.l.x;
            size_t index=(size_t)(local/selector->item_width);
            if(index<selector->item_count){
                selector_select(selector,index);
            }
        }
    }
    selector->last_left=pressed?1u:0u;
}

widget_selector_t *widget_create_raw_selector(void *parent,align_t align,float item_width,
    float height,float r,TTF_Font *font,const char *const *items,size_t count,
    unsigned track_color,unsigned selected_color,unsigned text_color,
    unsigned long animation_time,
    selector_callback_func callback,void *data){
    if(item_width<=0.0f||height<=0.0f||font==NULL||count==0){
        return NULL;
    }

    float width=item_width*(float)count;
    if(width<=0.0f){
        return NULL;
    }

    widget_selector_t *selector=(widget_selector_t *)create_layout(
        sizeof(*selector),
        parent,
        width,
        height,
        NULL,
        selector_update,
        selector_clear,
        selector_mouse,
        align
    );
    if(selector==NULL){
        return NULL;
    }

    selector->now_button=0;
    selector->controllable=1;
    selector->callback=callback;
    selector->data=data;
    selector->item_count=count;
    selector->item_width=item_width;
    selector->corner_radius=r;
    selector->from_x=0.0f;
    selector->to_x=0.0f;
    selector->last_left=0;
    selector->font=font;
    selector->item_height=height;
    selector->text_color=text_color;
    selector->track_color=track_color;
    selector->selected_color=selected_color;
    selector->animation_time=animation_time;

    selector->items=calloc(count,sizeof(*selector->items));
    if(selector->items==NULL){
        delete_layout((layout_t *)selector);
        return NULL;
    }
    for(size_t i=0;i<count;i++){
        const char *text=items!=NULL?items[i]:NULL;
        if(text!=NULL){
            selector->items[i]=selector_strdup(text);
            if(selector->items[i]==NULL){
                delete_layout((layout_t *)selector);
                return NULL;
            }
        }
    }

    align_t zero=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    layout_animation_t *animation=create_layout_animation(
        selector,selector_animation,
        draft_create_rrect(zero,width,height,r>=0.0f?r:height/2.0f,track_color,0),
        draft_create_rrect(zero,item_width,height,r>=0.0f?r:height/2.0f,selected_color,0),
        NULL
    );
    if(animation==NULL){
        delete_layout((layout_t *)selector);
        return NULL;
    }
    animation->timer=SDL_GetTicks();
    animation->lasting=0;
    animation->status=0;

    for(size_t i=0;i<count;i++){
        selector_label_t *label=create_layout(
            sizeof(*label),
            (layout_t *)selector,
            item_width,
            height,
            selector_label_draw,
            layout_update,
            selector_label_clear,
            NULL,
            make_align(1,3,0,1,item_width*((float)i-(float)count),0.0f,0.0f,1.0f)
        );
        if(label==NULL){
            delete_layout((layout_t *)selector);
            return NULL;
        }
        selector_label_apply(selector,label,i);
    }

    selector_update(selector);
    return selector;
}

void set_selector_item(widget_selector_t *selector,size_t i,const char *text){
    if(selector==NULL||i>=selector->item_count||selector->items==NULL){
        return;
    }

    char *copy=selector_strdup(text);
    if(text!=NULL&&copy==NULL){
        return;
    }
    free(selector->items[i]);
    selector->items[i]=copy;

    selector_label_t *label=selector_label_at(selector,i);
    if(label!=NULL){
        selector_label_apply(selector,label,i);
    }

    selector_update(selector);
    update_sons(selector);
}

void set_selector_item_width(widget_selector_t *selector,float width){
    if(selector==NULL||width<=0.0f||selector->item_count==0){
        return;
    }

    selector->item_width=width;
    selector->l.l.width=width*(float)selector->item_count;

    size_t index=0;
    for(size_t i=0;i<selector->l.l.son_size;i++){
        layout_t *son=selector->l.l.sons[i];
        if(son==NULL||son->draw!=selector_label_draw){
            continue;
        }
        selector_label_t *label=(selector_label_t *)son;
        label->l.width=width;
        label->l.align=make_align(1,3,0,1,
            width*((float)index-(float)selector->item_count),0.0f,0.0f,1.0f);
        selector_label_apply(selector,label,index);
        index++;
    }

    layout_animation_t *animation=widget_find_animation(selector);
    if(animation!=NULL){
        animation->lasting=0;
        animation->status=0;
    }
    selector->from_x=selector->l.l.x+(float)selector->now_button*width;
    selector->to_x=selector->from_x;

    selector_update(selector);
    update_sons(selector);
}

widget_selector_t *widget_create_selector(void *parent,align_t align,float item_width,
    selector_callback_func callback,void *data,...){
    if(item_width<=0.0f){
        return NULL;
    }

    darray_t(const char*) items;
    da_init(&items);

    va_list arg;
    va_start(arg,data);
    const char *text=NULL;
    while((text=va_arg(arg,const char *))!=NULL){
        if(da_add(sizeof(text),&items,&text)!=0){
            da_clear(&items);
            va_end(arg);
            return NULL;
        }
    }
    va_end(arg);

    if(items.size==0){
        da_clear(&items);
        return NULL;
    }

    TTF_Font *font=obtain_font(get_font(TEXT_CONTEXT),get_text_size(TEXT_CONTEXT));
    if(font==NULL){
        da_clear(&items);
        return NULL;
    }

    float height=get_theme()->button_height;
    if(get_theme()->item_height>0.0f&&height>get_theme()->item_height){
        height=get_theme()->item_height;
    }
    int line_height=TTF_GetFontHeight(font);
    if(line_height>0&&height<(float)line_height){
        height=(float)line_height;
    }

    widget_selector_t *selector=widget_create_raw_selector(
        parent,
        align,
        item_width,
        height,
        height/2.0f,
        font,
        items.arr,
        items.size,
        get_color(COLOR_SWITCH_OFF),
        get_color(COLOR_SWITCH_ON),
        get_text_color(TEXT_CONTEXT),
        get_theme()->switch_animation_time,
        callback,
        data
    );
    da_clear(&items);
    return selector;
}

widget_selector_t *widget_create_selector_title(void *parent,align_t align,float item_width,
    selector_callback_func callback,void *data,...){
    if(item_width<=0.0f){
        return NULL;
    }

    darray_t(const char*) items;
    da_init(&items);

    va_list arg;
    va_start(arg,data);
    const char *text=NULL;
    while((text=va_arg(arg,const char *))!=NULL){
        if(da_add(sizeof(text),&items,&text)!=0){
            da_clear(&items);
            va_end(arg);
            return NULL;
        }
    }
    va_end(arg);

    if(items.size==0){
        da_clear(&items);
        return NULL;
    }

    TTF_Font *font=obtain_font(get_font(TEXT_TITLE),get_text_size(TEXT_TITLE));
    if(font==NULL){
        da_clear(&items);
        return NULL;
    }

    float height=(float)TTF_GetFontHeight(font);
    if(height<=0.0f){
        height=get_theme()->button_height;
    }
    if(get_theme()->button_height>height){
        height=get_theme()->button_height;
    }

    widget_selector_t *selector=widget_create_raw_selector(
        parent,
        align,
        item_width,
        height,
        height/2.0f,
        font,
        items.arr,
        items.size,
        get_color(COLOR_SWITCH_OFF),
        get_color(COLOR_SWITCH_ON),
        get_text_color(TEXT_TITLE),
        get_theme()->switch_animation_time,
        callback,
        data
    );
    da_clear(&items);
    return selector;
}

/* ---- tabs ---- */

static void tabs_show_page(widget_tabs_t *tabs,size_t index){
    if(tabs==NULL){
        return;
    }
    tabs->now_page=index;

    for(size_t i=0;i<tabs->page_count;i++){
        layout_t *page=tabs->pages!=NULL?tabs->pages[i]:NULL;
        if(page==NULL){
            continue;
        }
        unsigned selected=(i==index)?1u:0u;
        page->enable=selected;
        page->active=selected;
    }
}

/* tabs 接管 selector 的 callback：先切页面，再转发给 selector 原来的 callback。 */
static void tabs_selector_callback(size_t now,size_t from,void *data){
    widget_tabs_t *tabs=data;
    if(tabs==NULL){
        return;
    }

    tabs_show_page(tabs,now);

    if(tabs->selector_callback!=NULL){
        tabs->selector_callback(now,from,tabs->selector_data);
    }
}

static void tabs_resize_selector(widget_tabs_t *tabs){
    if(tabs==NULL||tabs->selector==NULL||tabs->selector->item_count==0){
        return;
    }

    widget_selector_t *selector=tabs->selector;
    if(selector->l.l.parent!=(sublayout_t *)tabs){
        return;
    }
    float item_width=tabs->l.width/(float)selector->item_count;
    if(item_width<=0.0f){
        return;
    }

    float diff=item_width-selector->item_width;
    if(diff<0.0f){
        diff=-diff;
    }
    if(diff<=0.01f){
        return;
    }

    selector->item_width=item_width;
    selector->l.l.width=tabs->l.width;

    size_t label_index=0;
    for(size_t i=0;i<selector->l.l.son_size;i++){
        layout_t *label=selector->l.l.sons[i];
        if(label==NULL||label->draw!=selector_label_draw){
            continue;
        }
        label->width=item_width;
        label->align=make_align(1,3,0,1,
            item_width*((float)label_index-(float)selector->item_count),
            0.0f,0.0f,1.0f);
        selector_label_apply(selector,(selector_label_t *)label,label_index);
        label_index++;
    }

    layout_animation_t *animation=widget_find_animation(selector);
    if(animation==NULL||!animation->lasting){
        selector->from_x=selector->l.l.x+(float)selector->now_button*item_width;
        selector->to_x=selector->from_x;
    }
}

static void tabs_update(void *p){
    widget_tabs_t *tabs=p;
    if(tabs==NULL){
        return;
    }
    layout_update(&tabs->l);

    if(tabs->get_size!=NULL){
        float width=tabs->l.width;
        float height=tabs->l.height;
        tabs->get_size(&width,&height);
        if(width>0.0f){
            tabs->l.width=width;
        }
        if(height>0.0f){
            tabs->l.height=height;
        }
    }

    /* 只有 orphan selector 才由 tabs 接管布局；有父布局的 selector
     * 保持原来的位置和宽度。 */
    if(tabs->selector!=NULL&&tabs->selector->l.l.parent==(sublayout_t *)tabs){
        tabs->selector->l.l.x=tabs->l.x;
        tabs->selector->l.l.y=tabs->l.y;
        tabs->selector->l.l.width=tabs->l.width;
        tabs_resize_selector(tabs);
    }

    float top=tabs->l.y;
    if(tabs->selector!=NULL){
        float selector_bottom=tabs->selector->l.l.y+tabs->selector->l.l.height;
        if(selector_bottom>top){
            top=selector_bottom;
        }
    }
    float content_height=tabs->l.y+tabs->l.height-top;
    if(content_height<0.0f){
        content_height=0.0f;
    }
    for(size_t i=0;i<tabs->page_count;i++){
        layout_t *page=tabs->pages!=NULL?tabs->pages[i]:NULL;
        if(page==NULL){
            continue;
        }
        page->align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
        page->x=tabs->l.x;
        page->y=top;
        page->width=tabs->l.width;
        page->height=content_height;
    }

}

static void tabs_clear(void *p){
    widget_tabs_t *tabs=p;
    if(tabs==NULL){
        return;
    }

    /* 外部 selector 不归 tabs 所有，删除 tabs 时恢复它原来的 callback。
     * 如果 selector 是 tabs 的子布局，它已经先被 delete_layout() 删除，不能再访问。 */
    if(tabs->selector!=NULL&&!tabs->selector_attached){
        if(tabs->selector->callback==tabs_selector_callback&&
           tabs->selector->data==tabs){
            tabs->selector->callback=tabs->selector_callback;
            tabs->selector->data=tabs->selector_data;
        }
    }

    /* 页面是 tabs 的直接子布局，delete_layout() 已经先删掉它们；
     * 这里只释放页面指针数组。 */
    free(tabs->pages);
    tabs->pages=NULL;
    tabs->page_count=0;
    tabs->selector=NULL;
}

static int tabs_attach_page(widget_tabs_t *tabs,layout_t *page,size_t index){
    if(tabs==NULL||page==NULL){
        return 0;
    }
    if(!layout_attach(page,(layout_t *)tabs)){
        return 0;
    }

    page->align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    unsigned selected=(index==tabs->now_page)?1u:0u;
    page->enable=selected;
    page->active=selected;
    return 1;
}

void widget_set_tabs(widget_tabs_t *tabs,void *l,size_t n){
    if(tabs==NULL){
        return;
    }
    layout_t **new_pages=(layout_t **)l;
    if(n>0&&new_pages==NULL){
        return;
    }

    for(size_t i=0;i<tabs->page_count;i++){
        layout_t *old=tabs->pages!=NULL?tabs->pages[i]:NULL;
        if(old!=NULL){
            layout_detach(old);
            delete_layout(old);
        }
    }
    free(tabs->pages);
    tabs->pages=NULL;
    tabs->page_count=0;

    if(n==0){
        return;
    }

    tabs->pages=calloc(n,sizeof(*tabs->pages));
    if(tabs->pages==NULL){
        return;
    }
    tabs->page_count=n;

    for(size_t i=0;i<n;i++){
        layout_t *page=new_pages[i];
        if(page==NULL){
            continue;
        }
        if(tabs_attach_page(tabs,page,i)){
            tabs->pages[i]=page;
        }else{
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "widget_set_tabs: failed to attach page %zu",i);
        }
    }

    tabs_show_page(tabs,tabs->now_page);
    tabs_update(tabs);
}

void widget_set_tab(widget_tabs_t *tabs,void *l,size_t index){
    if(tabs==NULL){
        return;
    }

    if(index>=tabs->page_count){
        size_t old_count=tabs->page_count;
        size_t new_count=index+1;
        layout_t **pages=realloc(tabs->pages,new_count*sizeof(*pages));
        if(pages==NULL){
            return;
        }
        tabs->pages=pages;
        for(size_t i=old_count;i<new_count;i++){
            tabs->pages[i]=NULL;
        }
        tabs->page_count=new_count;
    }

    layout_t *old=tabs->pages[index];
    if(old!=NULL){
        layout_detach(old);
        delete_layout(old);
        tabs->pages[index]=NULL;
    }

    if(l==NULL){
        return;
    }

    layout_t *page=l;
    if(tabs_attach_page(tabs,page,index)){
        tabs->pages[index]=page;
    }else{
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "widget_set_tab: failed to attach page %zu",index);
    }
    tabs_update(tabs);
}

void widget_tabs_select(widget_tabs_t *tabs,size_t index){
    if(tabs==NULL){
        return;
    }

    if(tabs->selector!=NULL){
        if(index>=tabs->selector->item_count){
            return;
        }
        if(index==tabs->selector->now_button){
            tabs_show_page(tabs,index);
            return;
        }
        selector_select(tabs->selector,index);
        tabs_show_page(tabs,index);
        return;
    }

    tabs_show_page(tabs,index);
}

widget_tabs_t *widget_create_tabs(void *parent,align_t align,widget_size_func get_size,widget_selector_t *selector){
    if(selector==NULL){
        return NULL;
    }

    float width=0.0f;
    float height=0.0f;
    if(parent!=NULL){
        layout_t *p=parent;
        width=p->width;
        height=p->height;
    }
    if(get_size!=NULL){
        float wanted_w=width;
        float wanted_h=height;
        get_size(&wanted_w,&wanted_h);
        if(wanted_w>0.0f){
            width=wanted_w;
        }
        if(wanted_h>0.0f){
            height=wanted_h;
        }
    }
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    widget_tabs_t *tabs=(widget_tabs_t *)create_layout(
        sizeof(*tabs),
        parent,
        width,
        height,
        NULL,
        tabs_update,
        tabs_clear,
        NULL,
        align
    );
    if(tabs==NULL){
        return NULL;
    }

    tabs->get_size=get_size;
    tabs->pages=NULL;
    tabs->page_count=0;
    tabs->selector=selector;
    tabs->selector_callback=selector!=NULL?selector->callback:NULL;
    tabs->selector_data=selector!=NULL?selector->data:NULL;
    tabs->selector_attached=0;
    tabs->now_page=selector!=NULL?selector->now_button:0;

    if(selector->l.l.parent==NULL){
        if(!layout_attach((layout_t*)selector,(layout_t*)tabs)){
            delete_layout((layout_t*)tabs);
            return NULL;
        }
        selector->l.l.align=make_align(0,0,1,0,0.0f,0.0f,1.0f,0.0f);
        tabs->selector_attached=1;
    }else if(selector->l.l.parent==(sublayout_t *)tabs){
        tabs->selector_attached=1;
    }

    /* 保存原 callback 后覆盖为 tabs 的转发函数。 */
    selector->callback=tabs_selector_callback;
    selector->data=tabs;

    return tabs;
}

static void slider_update(void *p);
static void slider_draw(void *p);
static void slider_mouse(void *p);
static void slider_clear(void *p);

static float slider_view_size(const widget_slider_t *slider){
    return slider->direction?slider->l.width:slider->l.height;
}

static float slider_content_size(const widget_slider_t *slider){
    float view=slider_view_size(slider);
    return slider->all_height>view?slider->all_height:view;
}

static int slider_can_scroll(const widget_slider_t *slider){
    return slider->all_height>slider_view_size(slider);
}

static float slider_thumb_size(const widget_slider_t *slider){
    float view=slider_view_size(slider);
    float content=slider_content_size(slider);
    if(view<=0.0f){
        return 0.0f;
    }
    if(content<=view){
        return view;
    }

    float thumb=view*view/content;
    if(thumb<slider->least_height){
        thumb=slider->least_height;
    }
    return thumb>view?view:thumb;
}

static float slider_max_offset(const widget_slider_t *slider){
    float view=slider_view_size(slider);
    float content=slider_content_size(slider);
    return content>view?content-view:0.0f;
}

static void slider_clamp_offset(widget_slider_t *slider){
    float max=slider_max_offset(slider);
    if(slider->offset<0.0f){
        slider->offset=0.0f;
    }else if(slider->offset>max){
        slider->offset=max;
    }
}

/* 使用 color_gradient 从当前滑块色渐变到全透明；返回 0 表示完全透明。 */
static unsigned slider_color(widget_slider_t *slider){
    unsigned base=get_color(slider->dragging?COLOR_SLIDER_HIGHLIGHT:COLOR_SLIDER);
    if(slider->lasting_time==0){
        return base;
    }

    unsigned long now=SDL_GetTicks();
    if(now-slider->active_time<=slider->lasting_time){
        return base;
    }

    unsigned long elapsed=now-slider->active_time-slider->lasting_time;
    return color_gradient(0,base,elapsed,slider->slider_animation_time);
}

static void slider_update(void *p){
    widget_slider_t *slider=p;
    if(slider==NULL){
        return;
    }

    layout_update(&slider->l);
    if(slider->slider==NULL){
        return;
    }

    if(!slider_can_scroll(slider)){
        slider->offset=0.0f;
        slider->dragging=0;
        slider->active_time=SDL_GetTicks();
        slider->slider->color=0;
        return;
    }

    float view=slider_view_size(slider);
    float thumb=slider_thumb_size(slider);
    float max_offset=slider_max_offset(slider);
    slider_clamp_offset(slider);

    float mouse_x=get_mouse_x();
    float mouse_y=get_mouse_y();
    int inside=mouse_x>=slider->l.x&&mouse_x<=slider->l.x+slider->l.width
            && mouse_y>=slider->l.y&&mouse_y<=slider->l.y+slider->l.height;
    if(slider->dragging||inside){
        slider->active_time=SDL_GetTicks();
    }

    unsigned color=slider_color(slider);
    if((color&0xFF)==0){
        slider->slider->color=0;
        return;
    }

    float travel=view-thumb;
    float position=max_offset>0.0f?slider->offset/max_offset*travel:0.0f;

    if(slider->direction){
        slider->slider->x=slider->l.x+position;
        slider->slider->y=slider->l.y;
        slider->slider->width=thumb;
        slider->slider->height=slider->l.height;
    }else{
        slider->slider->x=slider->l.x;
        slider->slider->y=slider->l.y+position;
        slider->slider->width=slider->l.width;
        slider->slider->height=thumb;
    }

    float r=get_theme()->rect_round_corner;
    float limit=(slider->slider->width<slider->slider->height
                ?slider->slider->width:slider->slider->height)/2.0f;
    if(r>limit){
        r=limit;
    }
    slider->slider->r=r;
    slider->slider->auto_radius=0;
    slider->slider->color=color;
    slider->slider->border_color=0;
}

static void slider_draw(void *p){
    widget_slider_t *slider=p;
    if(slider==NULL||slider->slider==NULL||(slider->slider->color&0xFF)==0){
        return;
    }
    draft_draw_rrect(slider->slider);
}

static void slider_clear(void *p){
    widget_slider_t *slider=p;
    if(slider==NULL){
        return;
    }
    free(slider->slider);
    slider->slider=NULL;
}

void widget_slider_set(widget_slider_t *slider,float offset){
    if(slider==NULL){
        return;
    }
    slider->offset=offset;
    slider_clamp_offset(slider);
    slider_update(slider);
}

float widget_slider_get(const widget_slider_t *slider){
    return slider!=NULL?slider->offset:0.0f;
}

widget_slider_t *create_layout_slider(void *parent,align_t align,align_t slider_align,
    float all_height,int direction,slider_callback_func callback,void *data){
    (void)slider_align;

    layout_t *p=parent!=NULL?parent:get_root_layout();
    float thickness=get_theme()->slider_width;
    if(thickness<=0.0f){
        thickness=8.0f;
    }

    float width=direction?p->width:thickness;
    float height=direction?thickness:p->height;

    widget_slider_t *slider=create_layout(
        sizeof(widget_slider_t),
        p,
        width,
        height,
        slider_draw,
        slider_update,
        slider_clear,
        slider_mouse,
        align
    );
    if(slider==NULL){
        return NULL;
    }

    slider->all_height=all_height;
    slider->least_height=get_theme()->slider_least_height;
    slider->slider_animation_time=get_theme()->slider_animation_time;
    slider->lasting_time=0;
    slider->offset=0.0f;
    slider->direction=direction?1u:0u;
    slider->active_time=SDL_GetTicks();
    slider->dragging=0;
    slider->last_left=0;
    slider->callback=callback;
    slider->data=data;

    float thumb=slider_thumb_size(slider);
    align_t draft_align=make_align(0,0,0,0,0.0f,0.0f,0.0f,0.0f);
    slider->slider=draft_create_rrect(
        draft_align,
        direction?thumb:thickness,
        direction?thickness:thumb,
        get_theme()->rect_round_corner,
        get_color(COLOR_SLIDER),
        0
    );
    if(slider->slider==NULL){
        delete_layout((layout_t *)slider);
        return NULL;
    }

    slider_update(slider);
    return slider;
}

void slider_mouse(void *p){
    widget_slider_t *slider=p;
    if(slider==NULL){
        return;
    }

    int pressed=get_mouse_left();
    if(!slider_can_scroll(slider)){
        slider->dragging=0;
        slider->last_left=pressed?1u:0u;
        return;
    }

    float mouse_x=get_mouse_x();
    float mouse_y=get_mouse_y();
    int inside=mouse_x>=slider->l.x&&mouse_x<=slider->l.x+slider->l.width
            && mouse_y>=slider->l.y&&mouse_y<=slider->l.y+slider->l.height;

    if(pressed&&!slider->last_left&&inside){
        slider->dragging=1;
        slider->active_time=SDL_GetTicks();
    }

    if(slider->dragging&&pressed){
        float view=slider_view_size(slider);
        float thumb=slider_thumb_size(slider);
        float max_offset=slider_max_offset(slider);
        float travel=view-thumb;
        float axis=slider->direction?mouse_x:mouse_y;
        float start=slider->direction?slider->l.x:slider->l.y;
        float offset=0.0f;
        if(travel>0.0f&&max_offset>0.0f){
            float local=axis-start-thumb/2.0f;
            offset=local/travel*max_offset;
        }
        if(offset<0.0f){
            offset=0.0f;
        }else if(offset>max_offset){
            offset=max_offset;
        }

        if(offset!=slider->offset){
            slider->offset=offset;
            slider->active_time=SDL_GetTicks();
            slider_update(slider);
            if(slider->callback!=NULL){
                slider->callback(slider->offset,slider->data);
            }
        }
    }

    if(!pressed){
        slider->dragging=0;
    }
    slider->last_left=pressed?1u:0u;
}