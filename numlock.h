/* SLiM - Simple Login Manager
	Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
	Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.net>
	Copyright (C) 2012		Nobuhiro Iwamatsu <iwamatsu@nigauri.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
*/

#ifndef _NUMLOCK_H_
#define _NUMLOCK_H_

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

void NumLockSetOn(Display *dpy);
void NumLockSetOff(Display *dpy);

#endif /* _NUMLOCK_H_ */
