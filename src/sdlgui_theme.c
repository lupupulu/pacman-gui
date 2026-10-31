#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sdlgui_theme.h"
#include "sdlgui_i18n.h"
#include "CDS/darray.h"
#include "CDS/confc.h"


/* theme_t 里的 color/text 是 unsigned *，内建主题需要各自准备静态后备数组。 */
static unsigned builtin_bright_color[]={0xEFF1F5FF,0xCCD0DAFF,0x8CAAEEFF,0xFE640BFF,0xABC4FFFF};
static unsigned builtin_bright_text_color[]={0x4C4F69FF};
static unsigned builtin_dark_color[]={0x1E1E2EFF,0x313244FF,0x1E66F5FF,0xFAB387FF,0x89B4FAFF};
static unsigned builtin_dark_text_color[]={0xCDD6F4FF};
static unsigned builtin_text_size[]={20,16,14};
static char *builtin_font_names[]={NULL};   /* NULL 表示使用内嵌字体 */

/* 默认嵌入字体：由 tools/ttf_to_c.py 生成到 build/pacman_pixel_font.c。 */
extern const unsigned char pacman_pixel_ttf[];
extern const size_t pacman_pixel_ttf_len;

static theme_slice_t builtin_theme_slice[]={
    {
        .name="Bright",
        .color=builtin_bright_color,
        .color_size=5,
        .text_color=builtin_bright_text_color,
        .text_color_size=1,
        .divider_color=0xBCC0CCFF
    },
    {
        .name="Dark",
        .color=builtin_dark_color,
        .color_size=5,
        .text_color=builtin_dark_text_color,
        .text_color_size=1,
        .divider_color=0x45475AFF
    }
};

static theme_t theme={
    .version="0.0.1",
    .name=NULL,   /* NULL：内联的内建主题；theme_set 成功后才换成加载进来的名字 */
    .now_slice=&builtin_theme_slice[0],

    .fonts=builtin_font_names,
    .font_size=1,

    .text=builtin_text_size,
    .text_size=3,

    .item_height=32.0f,
    .button_animation_time=200,
    .button_height=32.0f,
    .rect_round_corner=10.0f,

    .slices=builtin_theme_slice,
    .slice_size=2
};

static toml_t theme_toml;


static void theme_free_loaded(void);   /* 定义在 theme_set 前面 */
static void release_font_cache(void);

/* 复制字符串；源为 NULL 时返回 NULL */
static char *theme_dup_string(const char *s){
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

/* 按指定字号打开内嵌字体，失败返回 NULL */
static TTF_Font *create_embedded_font(float ptsize){
    if(!TTF_WasInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_embedded_font: TTF is not initialized"));
        return NULL;
    }
    SDL_IOStream *io=SDL_IOFromConstMem(pacman_pixel_ttf,pacman_pixel_ttf_len);
    if(io==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_embedded_font: SDL_IOFromConstMem failed: %s"),SDL_GetError());
        return NULL;
    }
    TTF_Font *font=TTF_OpenFontIO(io,true,ptsize);
    if(font==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_embedded_font: TTF_OpenFontIO(%.0f) failed: %s"),ptsize,SDL_GetError());
    }else{
        TTF_SetFontHinting(font,TTF_HINTING_MONO);
    }
    return font;
}


/* 预热默认主题要用的内嵌字体；实际加载统一走 obtain_font() */
int theme_load_default_font(void){
    float ptsize=20.0f;
    if(theme.text!=NULL&&theme.text_size>0&&theme.text[0]>0){
        ptsize=(float)theme.text[0];
    }else{
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,_("No text sizes configured, using 20 pt"));
    }

    if(obtain_font(NULL,ptsize)==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("Failed to obtain the embedded default font at %.0f pt"),ptsize);
        return 0;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,_("Loaded embedded default font at %.0f pt"),ptsize);
    return 1;
}

/* ---- 主题读取 ----
   结构见 assets/Default/config.toml：根层是全局设置，每个 [表名] 是一套配色切片。 */

static const char *allow_version[]={"0.0.1"};

/* 复位成内联的内建默认主题（name=NULL 表示没有从配置文件加载） */
static void theme_reset_builtin(void){
    theme.name=NULL;
    theme.version="0.0.1";
    theme.now_slice=&builtin_theme_slice[0];
    theme.fonts=builtin_font_names;
    theme.font_size=1;
    theme.text=builtin_text_size;
    theme.text_size=3;
    theme.item_height=32.0f;
    theme.button_animation_time=200;
    theme.button_height=32.0f;
    theme.rect_round_corner=10.0f;
    theme.slices=builtin_theme_slice;
    theme.slice_size=2;
}

/* 释放主题里 malloc 出来的数组。
 * name/version/slice.name 指向 TOML 文档里的字符串，由 toml_unload 统一释放。
 * fonts 里的字体族名是 theme_set 复制出来的，需要逐个释放。 */
static void theme_free_arrays(theme_t *th){
    if(th->slices!=NULL){
        for(size_t i=0;i<th->slice_size;i++){
            free(th->slices[i].color);
            free(th->slices[i].text_color);
        }
        free(th->slices);
        th->slices=NULL;
    }
    if(th->fonts!=NULL&&th->fonts!=builtin_font_names){
        for(size_t i=0;i<th->font_size;i++){
            free(th->fonts[i]);
        }
        free(th->fonts);
        th->fonts=NULL;
    }
    free(th->text);
    th->text=NULL;
    th->slice_size=0;
    th->font_size=0;
    th->text_size=0;
}

/* 释放 theme_set 加载出来的部分，然后回到内联的内建默认主题。
 * name==NULL 就是内联主题的标记：没有加载数据，也不需要释放。 */
static void theme_free_loaded(void){
    if(theme.name==NULL){
        return;
    }
    theme_free_arrays(&theme);
    theme_reset_builtin();
}

/* 从 TOML 数值（整数或浮点）里取一个 double */
static int theme_read_number(toml_type_t *tp,double *out){
    if(tp==NULL){
        return 0;
    }
    if(*tp==TOML_INTEGER){
        *out=(double)((toml_integer_t *)tp)->v;
        return 1;
    }
    if(*tp==TOML_FLOAT){
        *out=((toml_float_t *)tp)->v;
        return 1;
    }
    return 0;
}

/* 颜色数组：每个元素都是 0xRRGGBBAA 范围内的整数 */
static unsigned *theme_read_colors(toml_type_t *tp,const char *slice,const char *key,size_t *out_size){
    *out_size=0;
    if(tp==NULL||*tp!=TOML_ARRAY){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: theme %s key %s must be an array"),slice,key);
        return NULL;
    }
    toml_array_t *arr=(toml_array_t *)tp;
    if(arr->size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: theme %s key %s must not be empty"),slice,key);
        return NULL;
    }

    unsigned *colors=malloc(arr->size*sizeof(*colors));
    if(colors==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: failed to allocate memory for %s.%s"),slice,key);
        return NULL;
    }

    for(size_t i=0;i<arr->size;i++){
        toml_type_t *item=arr->items[i];
        if(item==NULL||*item!=TOML_INTEGER){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: %s.%s[%zu] must be an integer"),slice,key,i);
            free(colors);
            return NULL;
        }
        long v=((toml_integer_t *)item)->v;
        if(v<0||v>0xFFFFFFFFL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: %s.%s[%zu] is outside 0x00000000..0xFFFFFFFF"),slice,key,i);
            free(colors);
            return NULL;
        }
        colors[i]=(unsigned)v;
    }

    *out_size=arr->size;
    return colors;
}

/* 单个颜色：0xRRGGBBAA 范围内的整数 */
static int theme_read_color(toml_type_t *tp,const char *slice,const char *key,unsigned *out){
    if(tp==NULL||*tp!=TOML_INTEGER){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s.%s must be an integer"),slice,key);
        return 0;
    }
    long v=((toml_integer_t *)tp)->v;
    if(v<0||v>0xFFFFFFFFL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s.%s is outside 0x00000000..0xFFFFFFFF"),slice,key);
        return 0;
    }
    *out=(unsigned)v;
    return 1;
}

/* 字号数组：元素是正数，写整数或浮点都可以 */
static unsigned *theme_read_text_sizes(toml_type_t *tp,size_t *out_size){
    *out_size=0;
    if(tp==NULL||*tp!=TOML_ARRAY){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: text_size must be an array"));
        return NULL;
    }
    toml_array_t *arr=(toml_array_t *)tp;
    if(arr->size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: text_size must not be empty"));
        return NULL;
    }

    unsigned *sizes=malloc(arr->size*sizeof(*sizes));
    if(sizes==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate memory for text_size"));
        return NULL;
    }

    for(size_t i=0;i<arr->size;i++){
        double v=0.0;
        if(!theme_read_number(arr->items[i],&v)){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: text_size[%zu] must be a number"),i);
            free(sizes);
            return NULL;
        }
        if(!isfinite(v)||v<1.0||v>1000.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: text_size[%zu] must be between 1 and 1000 pt"),i);
            free(sizes);
            return NULL;
        }
        sizes[i]=(unsigned)(v+0.5);
    }

    *out_size=arr->size;
    return sizes;
}



void theme_set(const char *path,const char *file){
    char *f=confc_find_path(path,file);
    if(f==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: cannot find %s under %s"),file?file:"(null)",path?path:"(null)");
        return ;
    }

    toml_t t=toml_load(f);
    free(f);
    if(t.size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to parse %s"),file);
        return ;
    }

    /* 新配置先完整读到 tmp，任何一项不合格都不碰正在使用的 theme/theme_toml。
       tmp.fonts 先保持 NULL，等 fonts 解析阶段再分配，避免失败时误释放当前主题的数组。 */
    theme_t tmp={0};
    tmp.item_height=32.0f;
    tmp.button_animation_time=200;
    tmp.button_height=32.0f;
    tmp.rect_round_corner=10.0f;

    toml_type_t *tp=NULL;

    /* 1. 版本必须认识 */
    tp=toml_lookup(&t,"",0,"version");
    if(tp==NULL||*tp!=TOML_STRING){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s is missing a string version"),file);
        goto fail;
    }
    const char *version=((toml_string_t *)tp)->str;
    int supported=0;
    for(size_t i=0;i<sizeof(allow_version)/sizeof(allow_version[0]);i++){
        if(strcmp(allow_version[i],version)==0){
            supported=1;
            break;
        }
    }
    if(!supported){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: unsupported configuration version %s"),version);
        goto fail;
    }
    tmp.version=(char *)version;

    /* 2. name 必选：只有内联主题（name==NULL）才没有名字 */
    tp=toml_lookup(&t,"",0,"name");
    if(tp==NULL||*tp!=TOML_STRING){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s is missing a string name"),file);
        goto fail;
    }
    tmp.name=((toml_string_t *)tp)->str;

    /* 3. 字号：没写就沿用内建的三级字号 */
    tp=toml_lookup(&t,"",0,"text_size");
    if(tp!=NULL){
        tmp.text=theme_read_text_sizes(tp,&tmp.text_size);
        if(tmp.text==NULL){
            goto fail;
        }
    }else{
        tmp.text=malloc(sizeof(builtin_text_size));
        if(tmp.text==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate default text sizes"));
            goto fail;
        }
        memcpy(tmp.text,builtin_text_size,sizeof(builtin_text_size));
        tmp.text_size=sizeof(builtin_text_size)/sizeof(builtin_text_size[0]);
    }

    /* 4. 字体族：写了就记下族名并通过 obtain_font 获取；没写就沿用当前字体 */
    tp=toml_lookup(&t,"",0,"fonts");
    if(tp!=NULL){
        if(*tp!=TOML_ARRAY){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: fonts must be an array"));
            goto fail;
        }
        toml_array_t *arr=(toml_array_t *)tp;
        if(arr->size==0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: fonts must not be empty"));
            goto fail;
        }

        tmp.fonts=calloc(arr->size,sizeof(*tmp.fonts));
        if(tmp.fonts==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate font array"));
            goto fail;
        }
        tmp.font_size=arr->size;

        for(size_t i=0;i<arr->size;i++){
            toml_type_t *item=arr->items[i];
            if(item==NULL||*item!=TOML_STRING||((toml_string_t *)item)->str[0]=='\0'){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("theme_set: fonts[%zu] must be a non-empty string"),i);
                goto fail;
            }
            const char *family=((toml_string_t *)item)->str;
            tmp.fonts[i]=theme_dup_string(family);
            if(tmp.fonts[i]==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("theme_set: failed to copy font name %s"),family);
                goto fail;
            }
            /* 统一从 obtain_font 入口取字体：命中缓存直接复用，找不到时回退内嵌字体 */
            if(obtain_font(family,(float)tmp.text[0])==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             _("theme_set: failed to obtain font %s"),family);
                goto fail;
            }
        }
    }else{
        /* 没写 fonts：复制当前字体族名，避免旧主题释放后留下悬空指针 */
        tmp.font_size=theme.font_size>0?theme.font_size:1;
        tmp.fonts=calloc(tmp.font_size,sizeof(*tmp.fonts));
        if(tmp.fonts==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate font array"));
            goto fail;
        }
        for(size_t i=0;i<tmp.font_size;i++){
            const char *family=(theme.fonts!=NULL&&i<theme.font_size)?theme.fonts[i]:NULL;
            if(family!=NULL){
                tmp.fonts[i]=theme_dup_string(family);
                if(tmp.fonts[i]==NULL){
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 _("theme_set: failed to copy font name %s"),family);
                    goto fail;
                }
            }
        }
    }

    /* 5. 尺寸参数 */
    tp=toml_lookup(&t,"",0,"item_height");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: item_height must be positive"));
            goto fail;
        }
        tmp.item_height=(float)v;
    }
    tp=toml_lookup(&t,"",0,"button_height");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: button_height must be positive"));
            goto fail;
        }
        tmp.button_height=(float)v;
    }
    tp=toml_lookup(&t,"",0,"button_animation_time");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: button_animation_time must be positive"));
            goto fail;
        }
        tmp.button_animation_time=(unsigned long)v;
    }
    if(tmp.button_height>tmp.item_height){
        tmp.button_height=tmp.item_height;
    }
    tp=toml_lookup(&t,"",0,"rect_round_corner");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: rect_round_corner must be non-negative"));
            goto fail;
        }
        tmp.rect_round_corner=(float)v;
    }

    /* 6. 根下每个非空名字的表都是一套配色切片 */
    size_t slice_count=0;
    for(size_t i=0;i<t.size;i++){
        toml_pair_t *pair=&t.pairs[i];
        if(pair->key==NULL||pair->key[0]=='\0'){
            continue;   /* "" 里放的是全局设置 */
        }
        if(pair->value!=NULL&&*pair->value==TOML_TABLE){
            slice_count++;
        }
    }
    if(slice_count==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     _("theme_set: %s contains no theme slices"),file);
        goto fail;
    }

    tmp.slices=calloc(slice_count,sizeof(*tmp.slices));
    if(tmp.slices==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: failed to allocate theme slices"));
        goto fail;
    }
    tmp.slice_size=slice_count;

    /* 先全部清零，失败时 theme_free_arrays 可以安全地按 slice_size 回收 */
    size_t si=0;
    for(size_t i=0;i<t.size;i++){
        toml_pair_t *pair=&t.pairs[i];
        if(pair->key==NULL||pair->key[0]=='\0'){
            continue;
        }
        if(pair->value==NULL||*pair->value!=TOML_TABLE){
            continue;
        }

        theme_slice_t *slice=&tmp.slices[si++];
        slice->name=pair->key;

        slice->color=theme_read_colors(toml_lookup_from_table((toml_table_t *)pair->value,"color"),
                                       slice->name,"color",&slice->color_size);
        if(slice->color==NULL){
            goto fail;
        }
        slice->text_color=theme_read_colors(toml_lookup_from_table((toml_table_t *)pair->value,"text_color"),
                                            slice->name,"text_color",&slice->text_color_size);
        if(slice->text_color==NULL){
            goto fail;
        }

        /* 未配置时回退到调色板的次要色 */
        slice->divider_color=(slice->color_size>COLOR_SECONDARY)?slice->color[COLOR_SECONDARY]:0x7F7F7FFF;
        tp=toml_lookup_from_table((toml_table_t *)pair->value,"divider_color");
        if(tp!=NULL&&!theme_read_color(tp,slice->name,"divider_color",&slice->divider_color)){
            goto fail;
        }
    }

    /* 7. default 必须指向已经读进来的某套切片；没写就用第一套 */
    tmp.now_slice=&tmp.slices[0];
    tp=toml_lookup(&t,"",0,"default");
    if(tp!=NULL){
        if(*tp!=TOML_STRING){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("theme_set: default must be a string"));
            goto fail;
        }
        const char *default_name=((toml_string_t *)tp)->str;
        int found=0;
        for(size_t i=0;i<tmp.slice_size;i++){
            if(strcmp(tmp.slices[i].name,default_name)==0){
                tmp.now_slice=&tmp.slices[i];
                found=1;
                break;
            }
        }
        if(!found){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         _("theme_set: default theme %s does not exist"),default_name);
            goto fail;
        }
    }

    /* 到这里 tmp 已经完整且有效，才释放旧配置并覆盖：
       旧数组先回收，再卸掉旧 TOML（tmp 里的字符串属于新 TOML，最后才接管）。
       字体本身归缓存所有，不在这里关闭。 */
    theme_free_loaded();
    toml_unload(&theme_toml);
    theme_toml=t;
    theme=tmp;

    /* 主题已经加载成功，这时才按环境 locale 启用 gettext */
    sdlgui_i18n_init();
    theme_select_system();

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                _("theme_set: loaded theme %s from %s (%zu slices, %zu fonts)"),
                theme.name,file,theme.slice_size,theme.font_size);
    return ;

fail:
    theme_free_arrays(&tmp);
    toml_unload(&t);
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                 _("theme_set: %s failed validation; keeping the current theme"),file);
}



int theme_select_slice(const char *name){
    if(name==NULL){
        return 0;
    }
    for(size_t i=0;i<theme.slice_size;i++){
        if(theme.slices[i].name!=NULL&&strcmp(theme.slices[i].name,name)==0){
            theme.now_slice=&theme.slices[i];
            return 1;
        }
    }
    return 0;
}

int theme_select_system(void){
    SDL_SystemTheme system_theme=SDL_GetSystemTheme();
    if(system_theme==SDL_SYSTEM_THEME_DARK){
        return theme_select_slice("Dark");
    }
    if(system_theme==SDL_SYSTEM_THEME_LIGHT){
        return theme_select_slice("Bright");
    }
    return 0;
}

unsigned get_color(size_t n){
    theme_slice_t *now=theme.now_slice;
    return now->color[n>=now->color_size?now->color_size-1:n];
}
unsigned get_text_color(size_t n){
    theme_slice_t *now=theme.now_slice;
    return now->text_color[n>=now->text_color_size?now->text_color_size-1:n];
}
unsigned get_divider_color(void){
    return theme.now_slice->divider_color;
}
const char *get_font(size_t n){
    return theme.fonts[n>=theme.font_size?theme.font_size-1:n];
}
float get_text_size(size_t n){
    return theme.text[n>=theme.text_size?theme.text_size-1:n];
}

const theme_t *get_theme(void){
    return &theme;
}
const theme_slice_t *get_theme_slice(void){
    return theme.now_slice;
}


void theme_quit(void){
    theme_free_loaded();
    toml_unload(&theme_toml);
    release_font_cache();
}


static TTF_Font *open_font_file(const char *name,const char *path,int index,float ptsize){
    if(path==NULL){
        return NULL;
    }

    SDL_PropertiesID props=SDL_CreateProperties();
    if(props==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to create font properties: %s"),SDL_GetError());
        return NULL;
    }

    TTF_Font *font=NULL;
    if(SDL_SetStringProperty(props,TTF_PROP_FONT_CREATE_FILENAME_STRING,path)&&
       SDL_SetFloatProperty(props,TTF_PROP_FONT_CREATE_SIZE_FLOAT,ptsize)&&
       SDL_SetNumberProperty(props,TTF_PROP_FONT_CREATE_FACE_NUMBER,(Sint64)index)){
        font=TTF_OpenFontWithProperties(props);
    }else{
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to set font properties: %s"),SDL_GetError());
    }
    SDL_DestroyProperties(props);

    if(font==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to open font %s (index %d): %s"),path,index,SDL_GetError());
    }else{
        TTF_SetFontHinting(font,TTF_HINTING_MONO);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,_("create_font: %s -> %s (index %d, %.0f pt)"),name,path,index,ptsize);
    }
    return font;
}

/* 通过 fontconfig 解析字体名，返回新字体以及解析出的路径/index（路径由调用方释放） */
static TTF_Font *create_font(const char *name,float ptsize,char **path_out,int *index_out){
    if(path_out!=NULL){
        *path_out=NULL;
    }
    if(index_out!=NULL){
        *index_out=0;
    }
    if(name==NULL||name[0]=='\0'){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: font name must not be empty"));
        return NULL;
    }
    if(!TTF_WasInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: TTF is not initialized"));
        return NULL;
    }

    /* 把 "字体族[:style=...]" 解析成 fontconfig 模式，再匹配到具体字体文件 */
    FcPattern *pattern=FcNameParse((const FcChar8 *)name);
    if(pattern==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to parse font pattern %s"),name);
        return NULL;
    }
    FcConfigSubstitute(NULL,pattern,FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result=FcResultMatch;
    FcPattern *match=FcFontMatch(NULL,pattern,&result);
    FcPatternDestroy(pattern);
    if(match==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: fontconfig could not find font %s (result %d)"),name,(int)result);
        return NULL;
    }

    FcChar8 *file=NULL;
    if(FcPatternGetString(match,FC_FILE,0,&file)!=FcResultMatch||file==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: match for font %s has no file path"),name);
        FcPatternDestroy(match);
        return NULL;
    }

    /* .ttc/.otc 这类集合要带上 face index，否则只会打开第 0 个面 */
    int index=0;
    FcPatternGetInteger(match,FC_INDEX,0,&index);

    char *path=theme_dup_string((const char *)file);
    FcPatternDestroy(match);
    if(path==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("create_font: failed to copy font path"));
        return NULL;
    }

    TTF_Font *font=open_font_file(name,path,index,ptsize);
    if(font==NULL){
        free(path);
        return NULL;
    }

    if(path_out!=NULL){
        *path_out=path;
    }else{
        free(path);
    }
    if(index_out!=NULL){
        *index_out=index;
    }
    return font;
}

typedef struct {
    char *name;    /* NULL 表示内嵌字体 */
    float ptsize;

    char *path;    /* fontconfig 解析出的字体文件；内嵌/回退字体为 NULL */
    int index;     /* .ttc/.otc 的 face index */
    TTF_Font *font;
}font_pair;

static darray_t(font_pair) font_cache;

/* NULL 字体名排在最前，其余按字符串比较 */
static int font_cache_name_cmp(const char *a,const char *b){
    if(a==NULL){
        return b==NULL?0:-1;
    }
    if(b==NULL){
        return 1;
    }
    return strcmp(a,b);
}

static int font_pair_cmp(const font_pair *a,const font_pair *b){
    int name_cmp=font_cache_name_cmp(a->name,b->name);
    if(name_cmp!=0){
        return name_cmp;
    }
    if(a->ptsize<b->ptsize){
        return -1;
    }
    if(a->ptsize>b->ptsize){
        return 1;
    }
    return 0;
}

/* 缓存按 (name, ptsize) 升序排列，返回第一个不小于 key 的位置 */
static size_t font_cache_lower_bound(const font_pair *key){
    size_t lo=0;
    size_t hi=font_cache.size;
    while(lo<hi){
        size_t mid=lo+(hi-lo)/2;
        if(font_pair_cmp(&font_cache.arr[mid],key)<0){
            lo=mid+1;
        }else{
            hi=mid;
        }
    }
    return lo;
}

/* 精确查找 (name, ptsize) */
static font_pair *font_cache_find(const char *name,float ptsize){
    font_pair key={.name=(char *)name,.ptsize=ptsize};
    size_t i=font_cache_lower_bound(&key);
    if(i<font_cache.size&&font_pair_cmp(&font_cache.arr[i],&key)==0){
        return &font_cache.arr[i];
    }
    return NULL;
}

/* 查找同名且已经记录文件路径的项，用于换字号时跳过 fontconfig */
static font_pair *font_cache_find_named(const char *name){
    font_pair key={.name=(char *)name,.ptsize=0.0f};
    size_t i=font_cache_lower_bound(&key);
    while(i<font_cache.size&&font_cache_name_cmp(font_cache.arr[i].name,name)==0){
        if(font_cache.arr[i].path!=NULL){
            return &font_cache.arr[i];
        }
        i++;
    }
    return NULL;
}

/* 二分插入，保持缓存按 (name, ptsize) 有序 */
static int font_cache_insert(font_pair *entry){
    font_pair key={.name=entry->name,.ptsize=entry->ptsize};
    size_t pos=font_cache_lower_bound(&key);
    return da_add_loc(sizeof(*entry),&font_cache,entry,pos);
}

/* 关闭缓存里的所有字体；必须在 TTF_Quit() 之前调用 */
static void release_font_cache(void){
    for(size_t i=0;i<font_cache.size;i++){
        if(font_cache.arr[i].font!=NULL){
            TTF_CloseFont(font_cache.arr[i].font);
        }
        free(font_cache.arr[i].name);
        free(font_cache.arr[i].path);
    }
    da_clear(&font_cache);
}

TTF_Font *obtain_font(const char *name,float ptsize){
    if(name!=NULL&&name[0]=='\0'){
        name=NULL;
    }
    if(!isfinite(ptsize)||ptsize<=0.0f){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: invalid point size %.1f"),(double)ptsize);
        return NULL;
    }

    font_pair *hit=font_cache_find(name,ptsize);
    if(hit!=NULL){
        return hit->font;
    }

    TTF_Font *font=NULL;
    char *path=NULL;
    int index=0;

    /* 同名不同字号：已有文件路径就直接按路径打开，不再查 fontconfig */
    font_pair *named=(name!=NULL)?font_cache_find_named(name):NULL;
    if(named!=NULL){
        font=open_font_file(name,named->path,named->index,ptsize);
        if(font!=NULL){
            path=theme_dup_string(named->path);
            index=named->index;
            if(path==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: failed to copy font path"));
                TTF_CloseFont(font);
                return NULL;
            }
        }
    }

    /* 没有记录过路径时才走 fontconfig */
    if(font==NULL&&name!=NULL&&named==NULL){
        font=create_font(name,ptsize,&path,&index);
    }

    if(font==NULL){
        font=create_embedded_font(ptsize);
        if(font==NULL){
            free(path);
            return NULL;
        }
        if(name!=NULL){
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        _("obtain_font: font %s not found; using the embedded font"),name);
        }
        free(path);
        path=NULL;
        index=0;
    }

    char *name_copy=theme_dup_string(name);
    if(name!=NULL&&name_copy==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: failed to copy font name"));
        free(path);
        TTF_CloseFont(font);
        return NULL;
    }

    font_pair entry={
        .name=name_copy,
        .ptsize=ptsize,
        .path=path,
        .index=index,
        .font=font
    };
    if(font_cache_insert(&entry)!=0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,_("obtain_font: failed to grow the font cache"));
        free(entry.name);
        free(entry.path);
        TTF_CloseFont(entry.font);
        return NULL;
    }
    return font;
}
