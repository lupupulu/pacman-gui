#ifndef SDLGUI_I18N_H
#define SDLGUI_I18N_H

#include <libintl.h>

#ifndef SDLGUI_TEXT_DOMAIN
#define SDLGUI_TEXT_DOMAIN "pacman-gui"
#endif

/* 相对当前目录的本地化目录；打包时可用 -DSDLGUI_LOCALEDIR=... 覆盖 */
#ifndef SDLGUI_LOCALEDIR
#define SDLGUI_LOCALEDIR "locale"
#endif

#define _(String) gettext(String)

/* 按环境 locale 启用 gettext；由 theme_set() 成功加载主题后调用 */
void sdlgui_i18n_init(void);

#endif
