/* SLiM - Simple Login Manager
   Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
   Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.net>
   Copyright (C) 2012-13 Nobuhiro Iwamatsu <iwamatsu@nigauri.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include <fstream>
#include <string>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "cfg.h"

using namespace std;

typedef pair<string,string> option;

Cfg::Cfg()
	: currentSession(-1)
{
	/* Configuration options */
	options.insert(option("default_path","/bin:/usr/bin:/usr/local/bin"));
	options.insert(option("default_xserver","/usr/bin/X"));
	options.insert(option("xserver_arguments",""));
	options.insert(option("numlock",""));
	options.insert(option("daemon",""));
	options.insert(option("xauth_path","/usr/bin/xauth"));
	options.insert(option("login_cmd","exec /bin/bash -login ~/.xinitrc %session"));
	options.insert(option("halt_cmd","/sbin/shutdown -h now"));
	options.insert(option("reboot_cmd","/sbin/shutdown -r now"));
	options.insert(option("suspend_cmd",""));
	options.insert(option("sessionstart_cmd",""));
	options.insert(option("sessionstop_cmd",""));
	options.insert(option("console_cmd","/usr/bin/xterm -C -fg white -bg black +sb -g %dx%d+%d+%d -fn %dx%d -T ""Console login"" -e /bin/sh -c ""/bin/cat /etc/issue; exec /bin/login"""));
	options.insert(option("screenshot_cmd","import -window root /slim.png"));
	options.insert(option("welcome_msg","Welcome to %host"));
	options.insert(option("session_msg","Session:"));
	options.insert(option("default_user",""));
	options.insert(option("focus_password","no"));
	options.insert(option("auto_login","no"));
	options.insert(option("current_theme","default"));
	options.insert(option("lockfile","/var/run/slim.lock"));
	options.insert(option("logfile","/var/log/slim.log"));
	options.insert(option("authfile","/var/run/slim.auth"));
	options.insert(option("shutdown_msg","The system is halting..."));
	options.insert(option("reboot_msg","The system is rebooting..."));
	options.insert(option("sessiondir",""));
	options.insert(option("hidecursor","false"));

	/* Theme stuff */
	options.insert(option("input_panel_x","50%"));
	options.insert(option("input_panel_y","40%"));
	options.insert(option("input_name_x","200"));
	options.insert(option("input_name_y","154"));
	options.insert(option("input_pass_x","-1")); /* default is single inputbox */
	options.insert(option("input_pass_y","-1"));
	options.insert(option("input_font","Verdana:size=11"));
	options.insert(option("input_color", "#000000"));
	options.insert(option("input_cursor_height","20"));
	options.insert(option("input_maxlength_name","20"));
	options.insert(option("input_maxlength_passwd","20"));
	options.insert(option("input_shadow_xoffset", "0"));
	options.insert(option("input_shadow_yoffset", "0"));
	options.insert(option("input_shadow_color","#FFFFFF"));

	options.insert(option("welcome_font","Verdana:size=14"));
	options.insert(option("welcome_color","#FFFFFF"));
	options.insert(option("welcome_x","-1"));
	options.insert(option("welcome_y","-1"));
	options.insert(option("welcome_shadow_xoffset", "0"));
	options.insert(option("welcome_shadow_yoffset", "0"));
	options.insert(option("welcome_shadow_color","#FFFFFF"));

	options.insert(option("intro_msg",""));
	options.insert(option("intro_font","Verdana:size=14"));
	options.insert(option("intro_color","#FFFFFF"));
	options.insert(option("intro_x","-1"));
	options.insert(option("intro_y","-1"));

	options.insert(option("background_style","stretch"));
	options.insert(option("background_color","#CCCCCC"));

	options.insert(option("username_font","Verdana:size=12"));
	options.insert(option("username_color","#FFFFFF"));
	options.insert(option("username_x","-1"));
	options.insert(option("username_y","-1"));
	options.insert(option("username_msg","Please enter your username"));
	options.insert(option("username_shadow_xoffset", "0"));
	options.insert(option("username_shadow_yoffset", "0"));
	options.insert(option("username_shadow_color","#FFFFFF"));

	options.insert(option("password_x","-1"));
	options.insert(option("password_y","-1"));
	options.insert(option("password_msg","Please enter your password"));

	options.insert(option("msg_color","#FFFFFF"));
	options.insert(option("msg_font","Verdana:size=16:bold"));
	options.insert(option("msg_x","40"));
	options.insert(option("msg_y","40"));
	options.insert(option("msg_shadow_xoffset", "0"));
	options.insert(option("msg_shadow_yoffset", "0"));
	options.insert(option("msg_shadow_color","#FFFFFF"));

	options.insert(option("session_color","#FFFFFF"));
	options.insert(option("session_font","Verdana:size=16:bold"));
	options.insert(option("session_x","50%"));
	options.insert(option("session_y","90%"));
	options.insert(option("session_shadow_xoffset", "0"));
	options.insert(option("session_shadow_yoffset", "0"));
	options.insert(option("session_shadow_color","#FFFFFF"));

	// slimlock-specific options
	options.insert(option("dpms_standby_timeout", "60"));
	options.insert(option("dpms_off_timeout", "600"));
	options.insert(option("wrong_passwd_timeout", "2"));
	options.insert(option("passwd_feedback_x", "50%"));
	options.insert(option("passwd_feedback_y", "10%"));
	options.insert(option("passwd_feedback_msg", "Authentication failed"));
	options.insert(option("passwd_feedback_capslock", "Authentication failed (CapsLock is on)"));
	options.insert(option("show_username", "1"));
	options.insert(option("show_welcome_msg", "0"));
	options.insert(option("tty_lock", "1"));
	options.insert(option("bell", "1"));
}

/*
 * Creates the Cfg object and parses
 * known options from the given configfile / themefile
 */
bool Cfg::readConf(const string& configfile) {
	string line, next;
	ifstream file(configfile);

	if (!file) {
		return false;
	}
	while (getline(file, line)) {
		auto pos = line.find('\\');
		if (pos != string::npos) {
			if (line.length() == pos + 1) {
				line.replace(pos, 1, " ");
				next = next + line;
				continue;
			} else
				line.replace(pos, line.length() - pos, " ");
		}

		if (!next.empty()) {
			line = next + line;
			next = "";
		}

		for(auto &opt_entry : options){
			auto op = opt_entry.first;
			if(line.find(op) == 0)
				options[op] = parseOption(line, op);
		}
	}
	file.close();

	fillSessionList();

	return true;
}

/* Returns the option value, trimmed */
string Cfg::parseOption(const string& line, const string& option ) {
	return Trim( line.substr(option.size(), line.size() - option.size()));
}

bool Cfg::optionTrue(string option) const {
	/* TODO: case insensitive comparison */
	return options.at(option) == "yes" || options.at(option) == "on" ||
		options.at(option) == "1" || options.at(option) == "true";
}

string& Cfg::getOption(string option) {
	return options[option];
}

/* return a trimmed string */
string Cfg::Trim( const string& s ) {
	auto pos1 = s.find_first_not_of(" \t\n\v\f\r");
	auto pos2 = s.find_last_not_of(" \t\n\v\f\r");
	if(pos1 == 0 && pos2 == s.length()-1){
		return s;
	}else{
		return s.substr(pos1,pos2-pos1+1);
	}
}

/* Return the welcome message with replaced vars */
string Cfg::getWelcomeMessage() {
	constexpr int MAX_NAME_LEN = 40;

	string s = getOption("welcome_msg");
	auto n = s.find("%host");
	if (n != string::npos) {
		string tmp = s.substr(0, n);
		char host[MAX_NAME_LEN];
		if(gethostname(host,MAX_NAME_LEN) == 0){
			tmp += host;
		}else{
			tmp += "<unknown hostname>";
		}
		tmp += s.substr(n+5);
		s = tmp;
	}
	n = s.find("%domain");
	if (n != string::npos) {
		string tmp = s.substr(0, n);;
		char domain[MAX_NAME_LEN];
		if(getdomainname(domain,MAX_NAME_LEN) == 0){
			tmp += domain;
		}else{
			tmp += "<unknown domain name>";
		}
		tmp += s.substr(n+7);
		s = tmp;
	}
	return s;
}

int Cfg::string2int(const char* string, bool* ok) {
	char* err = 0;
	int l = static_cast<int>(strtol(string, &err, 10));
	if (ok) {
		*ok = (*err == 0);
	}
	return (*err == 0) ? l : 0;
}

int Cfg::getIntOption(std::string option) const {
	return string2int(options.at(option).c_str());
}

/* Get absolute position */
int Cfg::absolutepos(const string& position, int max, int width) {
	auto n = position.find('%');
	if (n>0) { /* X Position expressed in percentage */
		int result = (max*string2int(position.substr(0, n).c_str())/100) - (width / 2);
		return result < 0 ? 0 : result ;
	} else { /* Absolute X position */
		return string2int(position.c_str());
	}
}

/* split a comma separated string into a vector of strings */
void Cfg::split(vector<string>& v, const string& str, char c, bool useEmpty) {
	v.clear();
	auto pos = str.find(c);
	size_t lastpos = 0;

	while (pos != string::npos) {
		auto tmp = str.substr(lastpos, pos-lastpos+1);
		
		if(useEmpty || tmp.length() > 0)
			v.push_back(tmp);

		lastpos = pos+1;
		pos = str.find(c, lastpos);
	}
	if(lastpos < str.length() - 1){
		v.push_back(str.substr(lastpos));
	}
	if(useEmpty){
		v.emplace_back("");
	}
}

void Cfg::fillSessionList(){
	auto strSessionDir = getOption("sessiondir");

	sessions.clear();

	if( !strSessionDir.empty() ) {
		DIR *pDir = opendir(strSessionDir.c_str());

		if (pDir != NULL) {
			struct dirent *pDirent = NULL;

			while ((pDirent = readdir(pDir)) != NULL) {
				auto strFile = strSessionDir + '/' + pDirent->d_name;
				struct stat oFileStat;

				if (stat(strFile.c_str(), &oFileStat) == 0) {
					if (S_ISREG(oFileStat.st_mode) && access(strFile.c_str(), R_OK) == 0){
						ifstream desktop_file( strFile );
						if (desktop_file){
							string line, session_name, session_exec;
							while (getline( desktop_file, line )) {
								if (line.substr(0, 5) == "Name=") {
									session_name = line.substr(5);
									if (!session_exec.empty())
										break;
								}else if(line.substr(0, 5) == "Exec=") {
									session_exec = line.substr(5);
									if (!session_name.empty())
										break;
								}
							}
							desktop_file.close();
							sessions.emplace_back(session_name, session_exec);
						}

					}
				}
			}
			closedir(pDir);
		}
	}

	if (sessions.empty()){
		sessions.emplace_back(string(), string());
	}
}

pair<string,string>& Cfg::nextSession() {
	currentSession = (currentSession + 1) % sessions.size();
	return sessions[currentSession];
}
