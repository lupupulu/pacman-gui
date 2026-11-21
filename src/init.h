#ifndef INIT_H
#define INIT_H

#include <stddef.h>

/* 创建主界面布局、selector 和 tabs；成功返回 1。
 * 调用前只需执行 pacman_init_all()；包列表由后台线程另行加载。 */
int init_ui(void);
/* 开始增量重建 installed 页：清空旧卡片并记录当前包总数。
 * 调用前必须确保后台枚举线程已经结束（例如 pthread_join）。 */
int init_ui_begin_refresh_packages(void);
/* 继续增量刷新：每次最多推进 max_items 个已枚举包；返回 1 表示还有未
 * 枚举的包。卡片由 installed_page_update() 每帧按限额补进列表。 */
int init_ui_refresh_packages_batch(size_t max_items);
/* 释放 pacman 后端和已安装包数据；应在界面销毁后调用。 */
void init_ui_quit(void);

#endif
