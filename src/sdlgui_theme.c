#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sdlgui_theme.h"
#include "CDS/confc.h"


/* theme_t 里的 color/text 是 unsigned *，内建主题需要各自准备静态后备数组。 */
static unsigned builtin_bright_color[]={0xFFFFFFFF,0x7F7F7FFF,0x1E90FFFF,0x1E90FFFF};
static unsigned builtin_bright_text_color[]={0x000000FF};
static unsigned builtin_dark_color[]={0x000000FF,0x7F7F7FFF,0x3399FFFF,0x1E90FFFF};
static unsigned builtin_dark_text_color[]={0x000000FF};
static unsigned builtin_text_size[]={20,16,14};

/* 默认嵌入字体：由 tools/ttf_to_c.py 生成到 build/pacman_pixel_font.c。 */
extern const unsigned char pacman_pixel_ttf[];
extern const size_t pacman_pixel_ttf_len;

static theme_slice_t builtin_theme_slice[]={
    {
        .name="Bright",
        .color=builtin_bright_color,
        .color_size=4,
        .text_color=builtin_bright_text_color,
        .text_color_size=1
    },
    {
        .name="Dark",
        .color=builtin_dark_color,
        .color_size=4,
        .text_color=builtin_dark_text_color,
        .text_color_size=1
    }
};

static theme_t theme={
    .version="0.0.1",
    .name=NULL,   /* NULL：内联的内建主题；theme_set 成功后才换成加载进来的名字 */
    .now_slice=&builtin_theme_slice[0],

    .fonts=NULL,
    .font_size=0,

    .text=builtin_text_size,
    .text_size=3,

    .item_height=32.0f,
    .rect_round_corner=0.0f,

    .slices=builtin_theme_slice,
    .slice_size=2
};

static toml_t theme_toml;

/* FcInit() 只需要成功一次；重复调用会让 fontconfig 反复加载配置而泄漏 */
static int fontconfig_ready=0;

static void theme_free_loaded(void);   /* 定义在 theme_set 前面 */

/* 关闭并释放一组字体。必须在 TTF_Quit() 之前调用。 */
static void free_fonts(TTF_Font **fonts,size_t size){
    if(fonts!=NULL){
        for(size_t i=0;i<size;i++){
            if(fonts[i]!=NULL){
                TTF_CloseFont(fonts[i]);
            }
        }
        free(fonts);
    }
}

/* 关闭并释放当前主题的字体 */
static void free_theme_fonts(void){
    free_fonts(theme.fonts,theme.font_size);
    theme.fonts=NULL;
    theme.font_size=0;
}

/* 把嵌入的默认字体打开一份，挂到默认主题上。
 * fonts 是字体族列表（默认配置里只有一个），text 是各级字号；
 * 这里用第一个字号打开，后续渲染可按需 TTF_SetFontSize()。 */
int theme_load_default_font(void){
    float ptsize=theme.text[0];
    if(theme.text!=NULL&&theme.text_size>0){
        ptsize=(float)theme.text[0];
    }else{
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"No text sizes configured, using 20 pt");
    }

    TTF_Font **fonts=calloc(1,sizeof(*fonts));
    if(fonts==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"calloc fonts failed: %s",SDL_GetError());
        return 0;
    }

    SDL_IOStream *io=SDL_IOFromConstMem(pacman_pixel_ttf,pacman_pixel_ttf_len);
    if(io==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"SDL_IOFromConstMem failed: %s",SDL_GetError());
        free(fonts);
        return 0;
    }

    fonts[0]=TTF_OpenFontIO(io,true,ptsize);
    if(fonts[0]==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"TTF_OpenFontIO(%.0f) failed: %s",ptsize,SDL_GetError());
        free(fonts);
        return 0;
    }

    theme.fonts=fonts;
    theme.font_size=1;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"Loaded embedded default font at %.0f pt",ptsize);
    return 1;
}

/* ---- 主题读取 ----
   结构见 assets/sdlgui_config.toml：根层是全局设置，每个 [表名] 是一套配色切片。 */

static const char *allow_version[]={"0.0.1"};

/* 复位成内联的内建默认主题（name=NULL 表示没有从配置文件加载） */
static void theme_reset_builtin(void){
    theme.name=NULL;
    theme.version="0.0.1";
    theme.now_slice=&builtin_theme_slice[0];
    theme.text=builtin_text_size;
    theme.text_size=3;
    theme.item_height=32.0f;
    theme.rect_round_corner=0.0f;
    theme.slices=builtin_theme_slice;
    theme.slice_size=2;
}

/* 释放主题里 malloc 出来的数组。
 * name/version/slice.name 指向 TOML 文档里的字符串，由 toml_unload 统一释放。 */
static void theme_free_arrays(theme_t *th){
    if(th->slices!=NULL){
        for(size_t i=0;i<th->slice_size;i++){
            free(th->slices[i].color);
            free(th->slices[i].text_color);
        }
        free(th->slices);
        th->slices=NULL;
    }
    free(th->text);
    th->text=NULL;
    th->slice_size=0;
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
                     "theme_set: 主题 %s 的 %s 必须是数组",slice,key);
        return NULL;
    }
    toml_array_t *arr=(toml_array_t *)tp;
    if(arr->size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "theme_set: 主题 %s 的 %s 不能为空",slice,key);
        return NULL;
    }

    unsigned *colors=malloc(arr->size*sizeof(*colors));
    if(colors==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "theme_set: 为 %s.%s 分配内存失败",slice,key);
        return NULL;
    }

    for(size_t i=0;i<arr->size;i++){
        toml_type_t *item=arr->items[i];
        if(item==NULL||*item!=TOML_INTEGER){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "theme_set: %s.%s[%zu] 必须是整数",slice,key,i);
            free(colors);
            return NULL;
        }
        long v=((toml_integer_t *)item)->v;
        if(v<0||v>0xFFFFFFFFL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "theme_set: %s.%s[%zu] 超出 0x00000000~0xFFFFFFFF",slice,key,i);
            free(colors);
            return NULL;
        }
        colors[i]=(unsigned)v;
    }

    *out_size=arr->size;
    return colors;
}

/* 字号数组：元素是正数，写整数或浮点都可以 */
static unsigned *theme_read_text_sizes(toml_type_t *tp,size_t *out_size){
    *out_size=0;
    if(tp==NULL||*tp!=TOML_ARRAY){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: text_size 必须是数组");
        return NULL;
    }
    toml_array_t *arr=(toml_array_t *)tp;
    if(arr->size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: text_size 不能为空");
        return NULL;
    }

    unsigned *sizes=malloc(arr->size*sizeof(*sizes));
    if(sizes==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: 为 text_size 分配内存失败");
        return NULL;
    }

    for(size_t i=0;i<arr->size;i++){
        double v=0.0;
        if(!theme_read_number(arr->items[i],&v)){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "theme_set: text_size[%zu] 必须是数字",i);
            free(sizes);
            return NULL;
        }
        if(!isfinite(v)||v<1.0||v>1000.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "theme_set: text_size[%zu] 必须在 1~1000 pt 之间",i);
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
                     "theme_set: 在 %s 下找不到 %s",path?path:"(null)",file?file:"(null)");
        return ;
    }

    toml_t t=toml_load(f);
    free(f);
    if(t.size==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: 解析 %s 失败",file);
        return ;
    }

    /* 新配置先完整读到 tmp，任何一项不合格都不碰正在使用的 theme/theme_toml */
    theme_t tmp={0};
    tmp.fonts=theme.fonts;           /* 默认沿用当前字体；配置里有 fonts 时再替换 */
    tmp.font_size=theme.font_size;
    tmp.item_height=32.0f;
    tmp.rect_round_corner=0.0f;

    toml_type_t *tp=NULL;

    /* 1. 版本必须认识 */
    tp=toml_lookup(&t,"",0,"version");
    if(tp==NULL||*tp!=TOML_STRING){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "theme_set: %s 缺少字符串类型的 version",file);
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
                     "theme_set: 不支持的配置版本 %s",version);
        goto fail;
    }
    tmp.version=(char *)version;

    /* 2. name 必选：只有内联主题（name==NULL）才没有名字 */
    tp=toml_lookup(&t,"",0,"name");
    if(tp==NULL||*tp!=TOML_STRING){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "theme_set: %s 缺少字符串类型的 name",file);
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
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: 分配默认字号失败");
            goto fail;
        }
        memcpy(tmp.text,builtin_text_size,sizeof(builtin_text_size));
        tmp.text_size=sizeof(builtin_text_size)/sizeof(builtin_text_size[0]);
    }

    /* 4. 字体族：写了就按名字用 fontconfig 找字体；没写就沿用当前字体 */
    tp=toml_lookup(&t,"",0,"fonts");
    if(tp!=NULL){
        if(*tp!=TOML_ARRAY){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: fonts 必须是数组");
            goto fail;
        }
        toml_array_t *arr=(toml_array_t *)tp;
        if(arr->size==0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: fonts 不能为空");
            goto fail;
        }

        tmp.fonts=calloc(arr->size,sizeof(*tmp.fonts));
        if(tmp.fonts==NULL){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: 分配字体数组失败");
            goto fail;
        }
        tmp.font_size=arr->size;

        for(size_t i=0;i<arr->size;i++){
            toml_type_t *item=arr->items[i];
            if(item==NULL||*item!=TOML_STRING||((toml_string_t *)item)->str[0]=='\0'){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "theme_set: fonts[%zu] 必须是非空字符串",i);
                goto fail;
            }
            const char *family=((toml_string_t *)item)->str;
            tmp.fonts[i]=create_font(family);
            if(tmp.fonts[i]==NULL){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "theme_set: 加载字体 %s 失败",family);
                goto fail;
            }
            /* create_font 用的是当前主题的字号，这里改成新主题的基础字号 */
            if(!TTF_SetFontSize(tmp.fonts[i],(float)tmp.text[0])){
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "theme_set: 设置字体 %s 的大小失败: %s",family,SDL_GetError());
                goto fail;
            }
        }
    }else{
        tmp.fonts=theme.fonts;
        tmp.font_size=theme.font_size;
    }

    /* 5. 尺寸参数 */
    tp=toml_lookup(&t,"",0,"item_height");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<=0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: item_height 必须是正数");
            goto fail;
        }
        tmp.item_height=(float)v;
    }
    tp=toml_lookup(&t,"",0,"rect_round_corner");
    if(tp!=NULL){
        double v=0.0;
        if(!theme_read_number(tp,&v)||!isfinite(v)||v<0.0){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: rect_round_corner 必须是非负数");
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
                     "theme_set: %s 里没有主题切片",file);
        goto fail;
    }

    tmp.slices=calloc(slice_count,sizeof(*tmp.slices));
    if(tmp.slices==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: 分配主题切片失败");
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
    }

    /* 7. default 必须指向已经读进来的某套切片；没写就用第一套 */
    tmp.now_slice=&tmp.slices[0];
    tp=toml_lookup(&t,"",0,"default");
    if(tp!=NULL){
        if(*tp!=TOML_STRING){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"theme_set: default 必须是字符串");
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
                         "theme_set: default 指定的主题 %s 不存在",default_name);
            goto fail;
        }
    }

    /* 到这里 tmp 已经完整且有效，才释放旧配置并覆盖：
       旧字体/旧数组先回收，再卸掉旧 TOML（tmp 里的字符串属于新 TOML，最后才接管） */
    if(tmp.fonts!=theme.fonts){
        free_theme_fonts();
    }
    theme_free_loaded();
    toml_unload(&theme_toml);
    theme_toml=t;
    theme=tmp;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "theme_set: 已从 %s 加载主题 %s（%zu 个切片，%zu 个字体）",
                file,theme.name,theme.slice_size,theme.font_size);
    return ;

fail:
    if(tmp.fonts!=theme.fonts){
        free_fonts(tmp.fonts,tmp.font_size);
    }
    theme_free_arrays(&tmp);
    toml_unload(&t);
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                 "theme_set: %s 未通过检查，继续使用当前主题",file);
}

void set_theme(const char *path,const char *file){
    theme_set(path,file);   /* 别名 */
}


unsigned get_color(size_t n){
    theme_slice_t *now=theme.now_slice;
    return now->color[n>=now->color_size?now->color_size-1:n];
}
unsigned get_text_color(size_t n){
    theme_slice_t *now=theme.now_slice;
    return now->text_color[n>=now->text_color_size?now->text_color_size-1:n];
}
TTF_Font *get_font(size_t n){
    return theme.fonts[n>=theme.font_size?theme.font_size-1:n];
}
float get_text_size(size_t n){
    return theme.text[n>=theme.text_size?theme.text_size-1:n];
}
float get_height(void){
    return theme.item_height;
}
float get_rrect(void){
    return theme.rect_round_corner;
}


TTF_Font *create_font(const char *name){
    if(name==NULL||name[0]=='\0'){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: 字体名不能为空");
        return NULL;
    }
    if(!TTF_WasInit()){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: TTF 尚未初始化");
        return NULL;
    }
    if(!fontconfig_ready){
        if(!FcInit()){
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: fontconfig 初始化失败");
            return NULL;
        }
        fontconfig_ready=1;
    }

    /* 把 "字体族[:style=...]" 解析成 fontconfig 模式，再匹配到具体字体文件 */
    FcPattern *pattern=FcNameParse((const FcChar8 *)name);
    if(pattern==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: 无法解析字体模式 %s",name);
        return NULL;
    }
    FcConfigSubstitute(NULL,pattern,FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result=FcResultMatch;
    FcPattern *match=FcFontMatch(NULL,pattern,&result);
    FcPatternDestroy(pattern);
    if(match==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: fontconfig 找不到字体 %s (result %d)",name,(int)result);
        return NULL;
    }

    FcChar8 *file=NULL;
    if(FcPatternGetString(match,FC_FILE,0,&file)!=FcResultMatch||file==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: 字体 %s 的匹配结果没有文件路径",name);
        FcPatternDestroy(match);
        return NULL;
    }

    /* .ttc/.otc 这类集合要带上 face index，否则只会打开第 0 个面 */
    int index=0;
    FcPatternGetInteger(match,FC_INDEX,0,&index);

    float ptsize=20.0f;
    if(theme.text!=NULL&&theme.text_size>0&&theme.text[0]>0){
        ptsize=(float)theme.text[0];
    }

    SDL_PropertiesID props=SDL_CreateProperties();
    if(props==0){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: 创建字体属性失败: %s",SDL_GetError());
        FcPatternDestroy(match);
        return NULL;
    }

    TTF_Font *font=NULL;
    if(SDL_SetStringProperty(props,TTF_PROP_FONT_CREATE_FILENAME_STRING,(const char *)file)&&
       SDL_SetFloatProperty(props,TTF_PROP_FONT_CREATE_SIZE_FLOAT,ptsize)&&
       SDL_SetNumberProperty(props,TTF_PROP_FONT_CREATE_FACE_NUMBER,(Sint64)index)){
        font=TTF_OpenFontWithProperties(props);
    }else{
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: 设置字体属性失败: %s",SDL_GetError());
    }
    SDL_DestroyProperties(props);

    if(font==NULL){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"create_font: 打开字体 %s (index %d) 失败: %s",(const char *)file,index,SDL_GetError());
    }else{
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,"create_font: %s -> %s (index %d, %.0f pt)",name,(const char *)file,index,ptsize);
    }

    FcPatternDestroy(match);
    return font;
}




void theme_quit(void){
    free_theme_fonts();
    theme_free_loaded();
    toml_unload(&theme_toml);
    if(fontconfig_ready){
        FcFini();
        fontconfig_ready=0;
    }
}
