/* SLiM - Simple Login Manager
   Copyright (C) 1997, 1998 Per Liden
   Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
   Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.net>
   Copyright (C) 2016 Christian Fiedler <fdlr.christian@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include <cassert>
#include <cstdio>
#include "switchuser.h"
#include "util.h"

using namespace std;

SwitchUser::SwitchUser(struct passwd *pw, Cfg *c, string display, char** _env) :
	cfg(c), pw(pw), displayName(move(display)), env(_env)
{
}

SwitchUser::~SwitchUser() 
{
	assert(0);
	/* Never called */
}

void SwitchUser::Login(const char* cmd, const char* mcookie)
{
	SetUserId();
	SetClientAuth(mcookie);
	Execute(cmd);
}

void SwitchUser::SwitchFailed()
{
	logStream << APPNAME << ": could not switch user id" << endl;
	exit(ERR_EXIT);
}

void SwitchUser::SetUserId()
{
	if(pw == nullptr){
		SwitchFailed();
		assert(0);
	}

	if(initgroups(pw->pw_name, pw->pw_gid) != 0){
		SwitchFailed();
		assert(0);
	}
	if(setgid(pw->pw_gid) != 0){
		SwitchFailed();
		assert(0);
	}
	if(setuid(pw->pw_uid) != 0){
		SwitchFailed();
		assert(0);
	}
}

void SwitchUser::Execute(const char* cmd)
{
	chdir(pw->pw_dir);
	execle(pw->pw_shell, pw->pw_shell, "-c", cmd, NULL, env);
	logStream << APPNAME << ": could not execute login command" << endl;
}

void SwitchUser::SetClientAuth(const char* mcookie)
{
	string home(pw->pw_dir);
	string authfile = home + "/.Xauthority";
	remove(authfile.c_str());
	Util::add_mcookie(mcookie, ":0", cfg->getOption("xauth_path"), authfile);
}
