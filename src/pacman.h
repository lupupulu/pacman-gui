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
    char *license;

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

    /* 枚举所有已安装包并通过 pkg_add() 加入全局 pkg_installed。 */
    void (*list_installed)(void);

    int (*install_package)(package_t *pkg);
    int (*update_package)(package_t *pkg);
    int (*uninstall_package)(package_t *pkg);
}pacman_t;

void pacman_register(pacman_t *m);

/* 后端列表：pacman.c 统一管理，init/main 不直接引用具体后端。 */
int pacman_init_all(void);
void pacman_quit_all(void);
size_t pacman_backend_count(void);
pacman_t *pacman_backend_at(size_t index);
/* 按 pacman_t.name 查找已注册后端；找不到返回 NULL。 */
pacman_t *pacman_backend_find(char *name);

/* installed 卡片使用的统一包视图：全局 pkg_installed 按 name 升序排列。
 * pacman_package_at() 返回的指针在下一次 pkg_add/pkg_del/pkg_del_from 前有效。 */
size_t pacman_package_count(void);
package_t *pacman_package_at(size_t index);

void pkg_stat_update(package_t *pkg);

/* pkg_add 接管 pkg->name/version/description/license/submitter 的所有权；
 * from/meta_data 视为后端借用的常量字符串。重复或插入失败时释放这些字段。 */
void pkg_add(package_t *pkg);

/* 按 from/name/meta_data/version 全部一致查找并删除；找不到时清空该管理器
 * 的包并重新调用它的 list_installed()。 */
void pkg_del(package_t *pkg);

/* 删除所有 from == name 的包。 */
void pkg_del_from(char *name);

#endif