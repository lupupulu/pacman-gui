#include <SDL3/SDL.h>
#include "sdlgui_widget.h"
#include <stdlib.h>


#define MAX(a,b) ((a)>(b)?(a):(b))

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
    int trigger_hold;        /* 触发色是否还在展示 */
    int pending_status;      /* 触发色结束后要切换到的状态 */
}button_anim_data_t;

static unsigned button_status_color(int status){
    if(status==BUTTON_STATUS_HOVER){
        return get_color(COLOR_BUTTON_HIGHLIGHT);
    }
    if(status==BUTTON_STATUS_TRIGGER){
        return get_color(COLOR_BUTTON_CONFIRM);
    }
    return get_color(COLOR_BUTTON);
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
            data->target_color=button_status_color(pending);
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
    unsigned long duration=get_theme()->button_animation_time;
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
    anim->target_color=button_status_color(status);
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

widget_button_t *widget_create_button(void *parent,float width,align_t align,const char *text,button_callback_func callback,void *data){
    float height=get_theme()->button_height;
    if(get_theme()->item_height>0.0f&&height>get_theme()->item_height){
        height=get_theme()->item_height;
    }
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    widget_button_t *widget=create_layout(
        sizeof(*widget),
        parent,
        width,
        height,
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

    layout_animation_t *animation=create_layout_animation(widget,button_animation,
        draft_create_rrect(full,width,height,get_theme()->rect_round_corner,get_color(COLOR_BUTTON),get_color(COLOR_BUTTON)),
        draft_create_text(
            make_align(3,3,0,0,0.0f,0.0f,0.0f,0.0f),
            obtain_font(get_font(TEXT_CONTEXT),
            get_text_size(TEXT_CONTEXT)),
            (char *)text,
            width,
            height,
            get_text_color(TEXT_CONTEXT)
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
    anim->origin_color=get_color(COLOR_BUTTON);
    anim->target_color=get_color(COLOR_BUTTON);
    anim->pending_status=BUTTON_STATUS_OUT;
    animation->data=anim;
    animation->l.clear=button_animation_clear;

    bbox_rrect_t *bbox=bbox_create_rrect(full,width,height,get_theme()->rect_round_corner);
    layout_button_t *button=create_layout_button(widget,width,height,full,button_callback,widget,bbox,NULL);
    (void)button;

    return widget;
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
static void switch_apply(switch_drafts_t *d,float left,float top,float width,float height,float knob_x,int on){
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
    track->r=track_h/2.0f;
    track->auto_radius=0;
    track->color=get_color(COLOR_SWITCH_OFF);
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
    on_block->r=track_h/2.0f;
    on_block->auto_radius=0;
    on_block->color=get_color(COLOR_SWITCH_ON);
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

    switch_apply(&d,left,sw->l.y,width,sw->l.height,d.knob!=NULL?d.knob->x:left,sw->on);
}

static void switch_animation(void *a){
    layout_animation_t *animation=a;
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

    float need=(float)get_theme()->switch_animation_time;
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

    widget_switch_t *sw=(widget_switch_t *)animation->l.parent;
    int on=sw!=NULL?sw->on:(animation->status==1);
    switch_apply(&d,left,animation->l.y,animation->l.width,animation->l.height,d.knob->x,on);
}
/* layout_button 只上报按钮状态，这里翻译成开关切换 */
static void switch_button_callback(int status,void *data){
    widget_switch_t *sw=data;
    if(sw==NULL||status!=BUTTON_STATUS_TRIGGER){
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

widget_switch_t *widget_create_switch(void *parent,align_t align,switch_callback_func callback,void *data){
    float track_h=get_theme()->button_height;
    float knob_h=get_theme()->switch_icon_height;
    if(knob_h<=0.0f){
        knob_h=track_h;
    }

    float height=MAX(track_h,knob_h);
    float width=get_theme()->switch_width;
    if(width<=0.0f||height<=0.0f){
        return NULL;
    }

    float track_r=track_h/2.0f;
    float knob_r=knob_h/2.0f;
    float track_w=knob_r>track_r?width-knob_r*2.0f+track_r*2.0f:width;
    if(track_w<=0.0f){
        track_w=width;
    }

    widget_switch_t *sw=create_layout(
        sizeof(*sw),
        parent,
        width,
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
    sw->callback=callback;
    sw->data=data;

    align_t full=make_align(3,3,1,1,0,0,1.0f,1.0f);
    bbox_rrect_t *bbox=bbox_create_rrect(full,width,height,height/2.0f);
    if(bbox==NULL){
        delete_layout((layout_t *)sw);
        return NULL;
    }
    layout_button_t *button=create_layout_button(sw,width,track_h,full,switch_button_callback,sw,bbox,NULL);
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
        draft_create_rrect(track_align,track_w,track_h,track_h/2.0f,get_color(COLOR_SWITCH_OFF),0),
        draft_create_rrect(on_align,0.0f,track_h,track_h/2.0f,get_color(COLOR_SWITCH_ON),0),
        /* 宽度 <=0 时按旋钮纹理原比例，用开关旋钮高度缩放 */
        draft_create_texture(knob_align,get_icon(ICON_KNOB),0.0f,knob_h),
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

int widget_switch_get(const widget_switch_t *sw){
    if(sw==NULL){
        return 0;
    }
    return sw->on?1:0;
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
