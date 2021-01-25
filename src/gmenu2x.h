/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo   *
 *   massimiliano.torromeo@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef GMENU2X_H
#define GMENU2X_H

class PowerManager;

#include <sys/stat.h>
#include <iostream>
#include <fcntl.h>
#include <string>
#include <vector>
#include <tr1/unordered_map>

#include "surfacecollection.h"
#include "iconbutton.h"
#include "translator.h"
#include "FastDelegate.h"
#include "utilities.h"
// #include "touchscreen.h"
#include "inputmanager.h"
#include "surface.h"
#include "fonthelper.h"
#include "debug.h"
#include "platform.h"

enum sb {
	SB_OFF,
	SB_LEFT,
	SB_BOTTOM,
	SB_RIGHT,
	SB_TOP,
	SB_CLASSIC,
};

enum bd {
	BD_OFF,
	BD_MENU,
	BD_DIALOG,
};

enum tvout {
	TV_OFF,
	TV_PAL,
	TV_NTSC,
};

using std::string;
using std::vector;
using fastdelegate::FastDelegate0;

typedef FastDelegate0<> MenuAction;
typedef unordered_map<string, string, hash<string> > ConfStrHash;
typedef unordered_map<string, int, hash<string> > ConfIntHash;
typedef unordered_map<string, RGBAColor, hash<string> > ConfColorHash;

struct MenuOption {
	string text;
	MenuAction action;
};

class Menu;

class GMenu2X {
private:
	string lastSelectorDir;
	int lastSelectorElement;
	void explorer();
	void readConfig(string conffile, bool defaults = false);
	bool readTmp();
	void initFont();
	void umountSdDialog();
	void opkInstall(string path);
	void pkgScanner();
	string ipkName(string cmd);
	void ipkInstall(string path);

public:
	static GMenu2X *instance;

	/*
	 * Variables needed for elements disposition
	 */
	SDL_Rect listRect, linksRect, sectionBarRect, bottomBarRect;

	//Configuration hashes
	ConfStrHash confStr, skinConfStr;
	ConfIntHash confInt, skinConfInt;
	ConfColorHash skinConfColor;

	SurfaceCollection sc;
	Surface *s, *bg;
	FontHelper *font = NULL, *titlefont = NULL;
	PowerManager *powerManager;
	Platform *platform;
	InputManager *input;
	Translator tr;
	// Touchscreen ts;
	Menu *menu;
	string currBackdrop = "", inetIcon = "";

	GMenu2X();
	~GMenu2X();
	void quit(bool all = true);
	void settings();
	void poweroffDialog();
	void resetSettings();
	void cpuSettings();
	void showManual();

	void setSkin(string skin, bool clearSC = true);
	void skinMenu();
	void skinColors();
	uint32_t updateSkin() { return 1; }
	string basenameFormatter(string value) { return base_name(value); }

	bool inputCommonActions(bool &inputAction);

	void cls(Surface *s = NULL, bool flip = true);

	void about();
	void viewLog();
	void contextMenu();
	void changeWallpaper();
	void changeSkin();
	void changeSelectorDir();

	bool saveScreenshot(string path);
	void drawSlider(int val, int min, int max, string icon, Surface *bg);

	void setInputSpeed();

	void writeConfig();
	void writeSkinConfig();
	void writeTmp(int selelem = -1, const string &selectordir = "");

	void initMenu();
	void addLink();
	void editLink();
	void deleteLink();
	void addSection();
	void renameSection();
	void deleteSection();

	string setBackground(Surface *bg, string wallpaper);

	int drawButton(Button *btn, int x = 5, int y = -8);
	int drawButton(Surface *s, const string &btn, const string &text = "", int x = 5, int y = -8);
	int drawButtonRight(Surface *s, const string &btn, const string &text = "", int x = 5, int y = -8);
	void drawScrollBar(uint32_t pagesize, uint32_t totalsize, uint32_t pagepos, SDL_Rect scrollRect, const uint8_t align = HAlignRight);

	static uint32_t timerFlip(uint32_t interval, void *param = NULL);

	int setVolume(int val, bool popup = false);
	int setBacklight(int val, bool popup = false);
};

#endif
