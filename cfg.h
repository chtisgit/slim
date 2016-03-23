/* SLiM - Simple Login Manager
   Copyright (C) 2004-06 Simone Rota <sip@varlock.com>
   Copyright (C) 2004-06 Johannes Winkelmann <jw@tks6.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#ifndef _CFG_H_
#define _CFG_H_

#include <string>
#include <map>
#include <vector>

#define INPUT_MAXLENGTH_NAME		30
#define INPUT_MAXLENGTH_PASSWD 		50

#define CFGFILE		(SYSCONFDIR "/slim.conf")
#define THEMESDIR	(PKGDATADIR "/themes")
#define THEMESFILE	"/slim.theme"

class Cfg {
	void fillSessionList();

	std::map<std::string,std::string> options;
	std::vector<std::pair<std::string,std::string> > sessions;
	int currentSession;

public:
	Cfg();

	bool readConf(const std::string& configfile);
	std::string parseOption(const std::string& line, const std::string& option);
	std::string& getOption(std::string option) ;
	bool optionTrue(std::string option) const;
	int getIntOption(std::string option) const;
	std::string getWelcomeMessage() ;

	static int absolutepos(const std::string &position, int max, int width);
	static int string2int(const char *string, bool *ok = 0);
	static void split(std::vector<std::string> &v, const std::string &str, 
					  char c, bool useEmpty=true);
	static std::string Trim(const std::string &s);

	std::pair<std::string,std::string>& nextSession();
};

#endif /* _CFG_H_ */
