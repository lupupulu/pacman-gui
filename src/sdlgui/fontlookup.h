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

#ifndef SDLGUI_FONTLOOKUP_H
#define SDLGUI_FONTLOOKUP_H

/* 字体族名 -> 字体文件的选择：
 *   - macOS 默认用 CoreText（系统自带，不需要 fontconfig）；
 *   - 其他平台用 fontconfig；
 *   - 定义 SDLGUI_USE_FONTCONFIG 可以强制使用 fontconfig。
 * 也按 fontconfig 的 "family:style=Bold" 语法解析 style。 */
#if defined(SDLGUI_USE_FONTCONFIG)
#define SDLGUI_FONT_BACKEND_FONTCONFIG 1
#elif defined(SDLGUI_USE_CORETEXT) || defined(__APPLE__)
#define SDLGUI_FONT_BACKEND_CORETEXT 1
#else
#define SDLGUI_FONT_BACKEND_FONTCONFIG 1
#endif

/* 把字体描述解析成字体文件路径；index 返回 .ttc/.otc 里的 face 序号。
 * 成功时返回 malloc 出来的路径，由调用方 free；找不到返回 NULL。
 * 支持 "monospace"、"sans-serif"、"serif" 这类通用族名。 */
char *sdlgui_font_lookup(const char *name,int *index);

#endif
