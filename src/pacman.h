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

typedef struct pacman_t pacman_t;

typedef struct {
    pacman_t *from;  // 所属后端；来自 AUR 的包由 AUR 后端提供
    char *name;
    char *meta_data;  // 用于标记来自flatpak等存在该条目的管理器

    char *description;
    draft_texture_t *icon; // 封面图，如果有的话
    char **licenses; // 以 NULL 结尾的许可证列表

    char *submitter;
    char **maintainers;
    char **contributors;

    char *version;
    char *new_version;        // 可更新的新版本，没有更新时为 NULL
    unsigned long epoch;

    double popularity;

    char *detail;

    pkg_stat_t stat;
    unsigned update_available:1; // 来自非 AUR 官方仓库且检测到新版本
    unsigned foreign:1; // 不在官方同步仓库（AUR/手动安装），不参与 pacman 操作
    double progress;
    unsigned long size; // 占用大小，未知时填(unsigned long)-1

    void *data; // 每个包管理器自有数据

}package_t;

struct pacman_t {
    char *name; // 管理器名

    /* 1 表示官方/非 AUR 仓库，允许用 pacman 检查更新、安装、删除。
     * AUR 或第三方后端应保持 0，自己不实现这些能力时 UI 不显示操作按钮。 */
    unsigned official:1;

    int (*init)(void);
    int (*init_settings)(void *tab);
    int (*refresh)(void); // 用于更新进度等
    int (*update)(void); // 更新源（对应pacman -Sy）
    int (*update_all)(void); // 更新所有包（对应pacman -Syu）
    void (*quit)(void);

    /* 检查该后端已安装包的更新；只应在后台线程或 UI 重建前调用。
     * 更新结果写入 package_t.update_available/new_version。 */
    int (*check_updates)(void);

    /* 枚举需要显示的已安装包并通过 pkg_add() 加入全局 pkg_installed。
     * 例如 alpm 只列出显式安装的包，自动依赖会被过滤掉。 */
    void (*list_installed)(void);

    /* 按关键字搜索可安装包，并通过 pkg_available_add() 加入 packages 栏目。
     * 关键字是用户输入的 UTF-8 文本；后端自己负责转义/过滤，禁止直接拼进
     * shell 命令。搜索通常由后台线程调用，返回时结果即已写入注册表。 */
    void (*search)(const char *keyword);

    /* 查询任意已安装包（包括作为依赖安装、未出现在 installed 页的包）。
     * 命中时填充 stat/version/foreign，并清空 update 字段；返回 1。 */
    int (*query_installed)(package_t *pkg);

    /* 只对官方/非 AUR 仓库实现；失败返回 0。 */
    int (*install_package)(package_t *pkg);
    int (*update_package)(package_t *pkg);
    int (*uninstall_package)(package_t *pkg);
};

void pacman_register(pacman_t *m);

/* 后端列表：pacman.c 统一管理，init/main 不直接引用具体后端。 */
/* 只初始化各后端，不枚举已安装包；枚举见 pacman_list_installed_all()。 */
int pacman_init_all(void);
/* 枚举所有后端的已安装包并写入 pkg_installed，然后调用
 * pacman_check_updates_all()。会修改全局包列表，调用期间 UI 不能并发
 * 读取；适合放到后台线程，完成后通知主线程刷新。 */
void pacman_list_installed_all(void);
/* 清空已安装注册表并重新枚举所有后端，然后检查更新。
 * 调用前必须先清空引用旧 package_t 的 UI 卡片。 */
void pacman_reload_installed_all(void);
/* 让支持 check_updates 的后端检查已安装包更新。 */
void pacman_check_updates_all(void);
/* 已安装注册表是否已经完整枚举并完成更新检查。装载期间 UI 不应读取
 * pkg_installed，也不应启动需要知道 foreign 状态的包操作。 */
int pacman_installed_ready(void);
/* 通过包所属后端查询它是否已安装（包括依赖包）。 */
int pacman_query_installed(package_t *pkg);
void pacman_quit_all(void);
size_t pacman_backend_count(void);
pacman_t *pacman_backend_at(size_t index);
/* 按 pacman_t.name 查找已注册后端；找不到返回 NULL。 */
pacman_t *pacman_backend_find(char *name);

/* installed 卡片使用的统一包视图：全局 pkg_installed 按 name 升序排列。
 * pacman_package_at() 返回的指针在下一次 pkg_add/pkg_del/pkg_del_from 前有效。 */
size_t pacman_package_count(void);
package_t *pacman_package_at(size_t index);
/* 按 from 后端指针+name 在已安装注册表里查找；找不到返回 NULL。 */
package_t *pacman_installed_find(const pacman_t *from,const char *name);

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


/* pkg_add 接管 pkg->name/version/new_version/description/licenses/submitter
 * 的所有权；from 是后端指针，meta_data 是后端借用的字符串。重复或插入失败时
 * 释放这些字段。 */
void pkg_add(package_t *pkg);

/* 按 from/name/meta_data/version 全部一致查找并删除；找不到时清空该管理器
 * 的包并重新调用它的 list_installed()。 */
void pkg_del(package_t *pkg);

/* 删除所有 from == 给定后端的包。 */
void pkg_del_from(pacman_t *from);

/* ---- 安装/更新/删除操作 ----
 * 这些操作由 package_t.from 指向的后端执行，只对 official（非 AUR）
 * 后端开放。操作在后台线程里运行，主线程通过 pacman_action_poll() 收尾。 */
typedef enum{
    PACMAN_ACTION_INSTALL,
    PACMAN_ACTION_UPDATE,
    PACMAN_ACTION_UNINSTALL
}pacman_action_t;

/* 启动一个后台操作；已有操作在跑、后端不支持或包名非法时返回 0。
 * 包的 name 等字段在启动时会复制，后台线程不会继续引用注册表里的
 * package_t，因此搜索/刷新可以同时发生。 */
int pacman_action_start(package_t *pkg,pacman_action_t action);
/* 是否有操作仍在运行。 */
int pacman_action_busy(void);
/* 主线程轮询：操作完成并 join 后返回 1，调用方应据此重建 UI。 */
int pacman_action_poll(void);

#endif