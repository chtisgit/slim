/* slimlock
 * Copyright (c) 2010-2012 Joel Burget <joelburget@gmail.com>
 * Copyright (c) 2016 Christian Fiedler <fdlr.christian@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>
#include <security/pam_appl.h>
#include <thread>
#include <err.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/file.h>
#include <fcntl.h>

#ifdef __linux__
#include <linux/vt.h>
#endif

#include "cfg.h"
#include "util.h"
#include "panel.h"

using namespace std;

/* CONSTANTS */

#undef APPNAME
#define APPNAME		"slimlock"
#define SLIMLOCKCFG	(SYSCONFDIR "/slimlock.conf")

#ifndef LOCKDIR

#if defined(__linux__)
#define LOCKDIR		"/var/lock"
#elif defined(__unix__)
#define LOCKDIR		"/var/run"
#else
#error "LOCKDIR not defined"
#endif

#endif /* LOCKDIR */

#define LOCKFILEPATH	(LOCKDIR "/" APPNAME ".lock")

#define DEV_CONSOLE	"/dev/console"

/* GLOBALS */

sig_atomic_t terminated = 0;

/* FUNCTIONS */

static void HideCursor(Cfg& cfg, Display *dpy, Window win)
{
	if (cfg.optionTrue("hidecursor")) {
		XColor black;
		char cursordata[1];
		Pixmap cursorpixmap;
		Cursor cursor;
		cursordata[0] = 0;
		cursorpixmap = XCreateBitmapFromData(dpy, win, cursordata, 1, 1);
		black.red = 0;
		black.green = 0;
		black.blue = 0;
		cursor = XCreatePixmapCursor(dpy, cursorpixmap, cursorpixmap,
									 &black, &black, 0, 0);
		XFreePixmap(dpy, cursorpixmap);
		XDefineCursor(dpy, win, cursor);
	}
}

static int ConvCallback(int num_msgs, const pam_message **msg,
						pam_response **resp, void *appdata_ptr)
{
	auto* loginPanel = static_cast<Panel*>(appdata_ptr);

	loginPanel->EventHandler(Panel::Get_Passwd);

	// PAM expects an array of responses, one for each message
	*resp = static_cast<pam_response*>( calloc(num_msgs, sizeof(pam_message)) );
	if (num_msgs == 0 || *resp == nullptr)
		return PAM_BUF_ERR;

	for (int i = 0; i < num_msgs; i++) {
		if (msg[i]->msg_style != PAM_PROMPT_ECHO_OFF &&
			msg[i]->msg_style != PAM_PROMPT_ECHO_ON)
			continue;

		// return code is currently not used but should be set to zero
		resp[i]->resp_retcode = 0;
		resp[i]->resp = strdup(loginPanel->GetPasswd().c_str());

		if (resp[i]->resp == nullptr){
			/* TODO: check if memory leak possible */
			free(*resp);
			return PAM_BUF_ERR;
		}
	}

	return PAM_SUCCESS;
}

static bool AuthenticateUser(pam_handle_t *pam_handle)
{
	return pam_authenticate(pam_handle, 0) == PAM_SUCCESS;
}

static string findValidRandomTheme(const string& set)
{
	// extract random theme from theme set; return empty string on error
	string name = set;
	struct stat buf;

	if (name[name.length() - 1] == ',') {
		name.erase(name.length() - 1);
	}

	Util::srandom(Util::makeseed());

	vector<string> themes;
	string themefile;
	Cfg::split(themes, name, ',');
	do {
		int sel = Util::random() % themes.size();

		name = Cfg::Trim(themes[sel]);
		themefile = string(THEMESDIR) +"/" + name + THEMESFILE;
		if (stat(themefile.c_str(), &buf) != 0) {
			themes.erase(find(themes.begin(), themes.end(), name));
			cerr << APPNAME << ": Invalid theme in config: "
				 << name << endl;
			name = "";
		}
	} while (name == "" && themes.size());
	return name;
}

static void die()
{
	exit(EXIT_FAILURE);
}

static void HandleSignal(int sig)
{
	terminated = 1;
}

static void setup_signal()
{
	void (*prev_fn)(int);

	// restore DPMS settings should slimlock be killed in the line of duty
	prev_fn = signal(SIGTERM, HandleSignal);
#if 0
	if (prev_fn == SIG_IGN) signal(SIGTERM, SIG_IGN);
#endif
}

/* CLASSES */

class LockFile{
	int fd;
public:
	LockFile(const char *path)
	{
		fd = open(path, O_CREAT | O_RDWR, 0666);
		cerr << "opening " << path << endl;
		if(fd != -1){
			if(flock(fd, LOCK_EX | LOCK_NB) != 0){
				close(fd);
				fd = -1;
			}
		}
	}
	operator bool()
	{
		return fd != -1;
	}
	~LockFile()
	{
		if(fd != -1){
			flock(fd, LOCK_UN);
			close(fd);
		}
	}
};

/* MAIN */

int main(int argc, char **argv)
{
	if((argc == 2) && strcmp("-v", argv[1]) == 0){
		cerr << APPNAME "-" VERSION ", © 2010-2012 Joel Burget" << endl;
		die();
	}else if(argc != 1){
		cerr << "usage: " APPNAME " [-v]" << endl;
		die();
	}

	setup_signal();

	// create a lock file to solve multiple instances problem
	LockFile lock_file(LOCKFILEPATH);

	if(!lock_file){
		cerr << APPNAME " already running" << endl;
		die();
	}

	// Read user's current theme
	Cfg cfg;
	cfg.readConf(CFGFILE);
	cfg.readConf(SLIMLOCKCFG);

	string themefile, themedir;
	string themebase( string(THEMESDIR) + '/' );
	string themeName( cfg.getOption("current_theme") );

	auto pos = themeName.find(",");
	if (pos != string::npos) {
		themeName = findValidRandomTheme(themeName);
	}

	bool loaded = false;
	while (!loaded) {
		themedir = themebase + themeName;
		themefile = themedir + THEMESFILE;
		if (!cfg.readConf(themefile)) {
			if (themeName == "default") {
				cerr << APPNAME << ": Failed to open default theme file "
					 << themefile << endl;
				die();
			} else {
				cerr << APPNAME << ": Invalid theme in config: "
					 << themeName << endl;
				themeName = "default";
			}
		} else {
			loaded = true;
		}
	}

	const char *display = getenv("DISPLAY");
	if (display == nullptr)
		display = DISPLAY;

	Display* dpy = XOpenDisplay(display);
	if(dpy == nullptr){
		cerr << APPNAME ": cannot open display" << endl;
		die();
	}
	int scr = DefaultScreen(dpy);

	XSetWindowAttributes wa;
	wa.override_redirect = 1;
	wa.background_pixel = BlackPixel(dpy, scr);

	// Create a full screen window
	Window root = RootWindow(dpy, scr);
	Window win = XCreateWindow(dpy,
		root,
		0, 0,
		DisplayWidth(dpy, scr),
		DisplayHeight(dpy, scr),
		0,
		DefaultDepth(dpy, scr),
		CopyFromParent,
		DefaultVisual(dpy, scr),
		CWOverrideRedirect | CWBackPixel,
		&wa);
	XMapWindow(dpy, win);

	XFlush(dpy);
	for (int len = 1000; len; len--) {
		if(XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
			== GrabSuccess)
			break;
		usleep(1000);
	}
	XSelectInput(dpy, win, ExposureMask | KeyPressMask);

	// This hides the cursor if the user has that option enabled in their
	// configuration
	HideCursor(cfg, dpy, win);

	Panel loginPanel(dpy, scr, win, &cfg, themedir, Panel::Mode_Lock);

	pam_handle_t *pam_handle;
	pam_conv conv = {ConvCallback, &loginPanel};

	int ret = pam_start(APPNAME, loginPanel.GetName().c_str(), &conv, &pam_handle);
	// If we can't start PAM, just exit because slimlock won't work right
	if (ret != PAM_SUCCESS){
		cerr << "PAM: " << pam_strerror(pam_handle, ret) << endl;
		die();
	}

	// disable tty switching
	int console;
	if(cfg.optionTrue("tty_lock")) {
		console = open(DEV_CONSOLE, O_RDWR);
		if (console == -1)
			perror("error opening console");

#ifdef __linux__
		if (ioctl(console, VT_LOCKSWITCH) == -1)
			perror("error locking console");
#else
		cerr << APPNAME ": tty_lock has no effect! (currently linux only)" << endl;
#endif
	}

	// Set up DPMS
	CARD16 dpms_standby, dpms_suspend, dpms_off, dpms_level;
	BOOL dpms_state, using_dpms;
	unsigned int cfg_dpms_standby, cfg_dpms_off;
	cfg_dpms_standby = Cfg::string2int(cfg.getOption("dpms_standby_timeout").c_str());
	cfg_dpms_off = Cfg::string2int(cfg.getOption("dpms_off_timeout").c_str());
	using_dpms = DPMSCapable(dpy) && (cfg_dpms_standby > 0);
	if (using_dpms) {
		DPMSGetTimeouts(dpy, &dpms_standby, &dpms_suspend, &dpms_off);

		DPMSSetTimeouts(dpy, cfg_dpms_standby,
						cfg_dpms_standby, cfg_dpms_off);

		DPMSInfo(dpy, &dpms_level, &dpms_state);
		if (!dpms_state)
			DPMSEnable(dpy);
	}

	// Get password timeout
	unsigned int cfg_passwd_timeout;
	cfg_passwd_timeout = Cfg::string2int(cfg.getOption("wrong_passwd_timeout").c_str());
	// Let's just make sure it has a sane value
	cfg_passwd_timeout = cfg_passwd_timeout > 60 ? 60 : cfg_passwd_timeout;

	thread raise_thread([&dpy,&win](){
		while(terminated == 0) {
			XRaiseWindow(dpy, win);
			sleep(1);
		}
	});

	// Main loop
	while (terminated == 0)
	{
		loginPanel.ResetPasswd();

		// AuthenticateUser returns true if authenticated
		if (AuthenticateUser(pam_handle))
			break;

		loginPanel.WrongPassword(cfg_passwd_timeout);
	}

	// join thread before destroying the window that it's supposed to be raising
	terminated = 1;
	raise_thread.join();

	loginPanel.ClosePanel();

	// Get DPMS stuff back to normal
	if (using_dpms) {
		DPMSSetTimeouts(dpy, dpms_standby, dpms_suspend, dpms_off);
		// turn off DPMS if it was off when we entered
		if (!dpms_state)
			DPMSDisable(dpy);
	}

	XCloseDisplay(dpy);

	if(cfg.optionTrue("tty_lock")) {
#ifdef __linux__
		if ((ioctl(console, VT_UNLOCKSWITCH)) == -1) {
			perror("error unlocking console");
		}
#endif
		close(console);
	}

	return 0;
}

