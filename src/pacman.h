#ifndef PACMAN_H
#define PACMAN_H

#include <stddef.h>
#include "sdlgui.h"

typedef enum{
    PKG_STAT_NULL,
    PKG_STAT_INSTALLED,
    PKG_STAT_INSTALLING,
    PKG_STAT_UPDATING,
    PKG_STAT_UNINSTALLING
}pkg_stat_t;

typedef struct {
    char *from;  // 标记来自pacman/aur或其他管理器
    char *name;
    char *meta_data;  // 用于标记来自flatpak等存在该条目的管理器

    char *description;
    draft_texture_t *icon; // 封面图，如果有的话
    char **licenses; // 以 NULL 结尾的许可证列表

    char *submitter;
    char **maintainers;
    char **contributors;

    char *version;
    unsigned long epoch;

    double popularity;

    char *detail;

    pkg_stat_t stat;
    double progress;
    unsigned long size; // 占用大小，未知时填(unsigned long)-1

    void *data; // 每个包管理器自有数据

}package_t;

typedef struct {
    char *name; // 管理器名

    int (*init)(void);
    int (*init_settings)(void *tab);
    int (*refresh)(void); // 用于更新进度等
    int (*update)(void); // 更新源（对应pacman -Sy）
    int (*update_all)(void); // 更新所有包（对应pacman -Syu）
    void (*quit)(void);

    /* 枚举需要显示的已安装包并通过 pkg_add() 加入全局 pkg_installed。
     * 例如 alpm 只列出显式安装的包，自动依赖会被过滤掉。 */
    void (*list_installed)(void);

    /* 按关键字搜索可安装包，并通过 pkg_available_add() 加入 packages 栏目。
     * 关键字是用户输入的 UTF-8 文本；后端自己负责转义/过滤，禁止直接拼进
     * shell 命令。搜索通常由后台线程调用，返回时结果即已写入注册表。 */
    void (*search)(const char *keyword);

    int (*install_package)(package_t *pkg);
    int (*update_package)(package_t *pkg);
    int (*uninstall_package)(package_t *pkg);
}pacman_t;

void pacman_register(pacman_t *m);

/* 后端列表：pacman.c 统一管理，init/main 不直接引用具体后端。 */
/* 只初始化各后端，不枚举已安装包；枚举见 pacman_list_installed_all()。 */
int pacman_init_all(void);
/* 枚举所有后端的已安装包并写入 pkg_installed。会修改全局包列表，
 * 调用期间 UI 不能并发读取；适合放到后台线程，完成后通知主线程刷新。 */
void pacman_list_installed_all(void);
void pacman_quit_all(void);
size_t pacman_backend_count(void);
pacman_t *pacman_backend_at(size_t index);
/* 按 pacman_t.name 查找已注册后端；找不到返回 NULL。 */
pacman_t *pacman_backend_find(char *name);

/* installed 卡片使用的统一包视图：全局 pkg_installed 按 name 升序排列。
 * pacman_package_at() 返回的指针在下一次 pkg_add/pkg_del/pkg_del_from 前有效。 */
size_t pacman_package_count(void);
package_t *pacman_package_at(size_t index);
/* 按 from+name 在已安装注册表里查找；找不到返回 NULL。 */
package_t *pacman_installed_find(const char *from,const char *name);

/* packages 栏目的可安装包视图：后端搜索结果，保持后端的返回顺序（不排序）。
 * 搜索在后台线程写注册表，主线程 join 之后才能读取。 */
size_t pacman_available_count(void);
package_t *pacman_available_at(size_t index);
/* 清空可安装注册表。 */
void pacman_available_clear(void);
/* 接管 pkg 的字符串字段；按 from+name+meta_data 去重，重复或失败时释放。
 * 新条目追加到末尾，结果顺序由后端决定。 */
void pkg_available_add(package_t *pkg);
/* 清空注册表并让所有支持搜索的后端按 keyword 填充。
 * 只应在后台线程调用；主线程 join 之后再读 pacman_available_*。 */
void pacman_search_all(const char *keyword);

/* 生成一张软件包卡片（首页 packages/installed 共用）。 */
layout_rect_t *create_layout_package(package_t *package);


/* pkg_add 接管 pkg->name/version/description/licenses/submitter 的所有权；
 * from/meta_data 视为后端借用的常量字符串。重复或插入失败时释放这些字段。 */
void pkg_add(package_t *pkg);

/* 按 from/name/meta_data/version 全部一致查找并删除；找不到时清空该管理器
 * 的包并重新调用它的 list_installed()。 */
void pkg_del(package_t *pkg);

/* 删除所有 from == name 的包。 */
void pkg_del_from(char *name);

#endif