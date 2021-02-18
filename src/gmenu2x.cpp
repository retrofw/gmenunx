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

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <stdlib.h>
#include <signal.h>

#include "linkapp.h"
#include "fonthelper.h"
#include "surface.h"
#include "browsedialog.h"
#include "powermanager.h"
#include "gmenu2x.h"
#include "filelister.h"
#include "iconbutton.h"
#include "messagebox.h"
#include "inputdialog.h"
#include "settingsdialog.h"
#include "wallpaperdialog.h"
#include "skindialog.h"
#include "textdialog.h"
#include "terminaldialog.h"
#include "menusettingint.h"
#include "menusettingbool.h"
#include "menusettingrgba.h"
#include "menusettingstring.h"
#include "menusettingmultistring.h"
#include "menusettingfile.h"
#include "menusettingimage.h"
#include "menusettingdir.h"
#include "imageviewerdialog.h"
#include "menusettingdatetime.h"
#include "debug.h"

#include "pkgscannerdialog.h"
#include "libopk.h"

using std::ifstream;
using std::ofstream;
using std::stringstream;
using namespace fastdelegate;

#define sync() sync(); system("sync &");

string prevDateTime = "", newDateTime = "";

#include "menu.h"

GMenu2X *GMenu2X::instance = NULL;

static void quit_all(int err) {
	delete GMenu2X::instance;
	INFO("Quitting GMenuNX...");
	exit(err);
}

int main(int /*argc*/, char * /*argv*/[]) {
	INFO("Starting GMenuNX...");

	signal(SIGINT,  &quit_all);
	signal(SIGSEGV, &quit_all);
	signal(SIGTERM, &quit_all);

	int fd = open("/dev/tty0", O_RDONLY);
	if (fd > 0) {
		ioctl(fd, VT_UNLOCKSWITCH, 1);
		ioctl(fd, KDSETMODE, KD_TEXT);
		ioctl(fd, KDSKBMODE, K_XLATE);
		close(fd);
	}

	usleep(1000);

	new GMenu2X();

	return 0;
}

GMenu2X::~GMenu2X() {
	writeConfig();
}

void GMenu2X::quit(bool all) {
	s->flip(); s->flip(); s->flip(); // flush buffers

	powerManager->clearTimer();

	writeConfig();

	delete input;
	delete menu;
	delete s;
	delete font;
	delete titlefont;

	SDL_Quit();

	fflush(NULL);

	delete platform;

	if (all) {
		quit_all(0);
	}
}

GMenu2X::GMenu2X() {
	GMenu2X::instance = this;

	platform = PlatformInit(this);

	mkdir(home_path().c_str(), 0777);

	readConfig(home_path("gmenunx.conf"), true);

	platform->setScaleMode(0);

	setBacklight(confInt["backlight"]);
	setVolume(confInt["globalVolume"]);

	setenv("SDL_FBCON_DONT_CLEAR", "1", 0);
	setenv("SDL_NOMOUSE", "1", 1);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) < 0) {
		ERROR("Could not initialize SDL: %s", SDL_GetError());
		quit();
		return;
	}
	SDL_WM_SetCaption("gmenunx", NULL);
	SDL_ShowCursor(SDL_DISABLE);

	input = new InputManager(this, home_path("input.conf"));

	setInputSpeed();

	s = new Surface(platform->w, platform->h, platform->bpp);

	setSkin(confStr["skin"], true);

	currBackdrop = confStr["wallpaper"];
	confStr["wallpaper"] = setBackground(s, currBackdrop);

	bg = new Surface(s);

	powerManager = new PowerManager(this, confInt["backlightTimeout"], confInt["powerTimeout"]);

	MessageBox mb(this, tr["Loading"]);
	mb.setAutoHide(1);
	mb.setBgAlpha(0);
	mb.exec();

	menu = new Menu(this);
	initMenu();

	if (readTmp() && confInt["outputLogs"]) {
		viewLog();
	};

	input->dropEvents();

	powerManager->resetSuspendTimer();

	// recover last session
	if (confInt["selectorElement"] >= 0 && menu->getLinkApp() != NULL && (!menu->getLinkApp()->getSelectorDir().empty() || !confStr["selectorDir"].empty())) {
		if (confInt["skinBackdrops"] & BD_DIALOG)
			setBackground(bg, currBackdrop);
		else
			setBackground(bg, confStr["wallpaper"]);
		menu->getLinkApp()->selector(confInt["selectorElement"], confStr["selectorDir"]);
	}

	menu->exec();
}

bool GMenu2X::inputCommonActions(bool &inputAction) {
	if (inputAction) powerManager->resetSuspendTimer();

	if (powerManager->suspendActive) {
		// SUSPEND ACTIVE
		while (!(input->isActive(POWER) || input->isActive(SETTINGS) || input->isActive(UDC_CONNECT) || input->isActive(UDC_REMOVE) || input->isActive(MMC_INSERT) || input->isActive(MMC_REMOVE))) {
			input->update();
		}

		powerManager->doSuspend(0);

		while (input->isActive(POWER) || input->isActive(SETTINGS)) {
			input->update();
		}

		input->setActive(WAKE_UP);
		return true;
	}

	uint32_t button_hold = SDL_GetTicks();

	int wasActive = 0;

	while (input->isActive(POWER)) { // POWER HOLD
		wasActive = POWER;

		input->update();

		if (SDL_GetTicks() - button_hold > 1000) {
			wasActive = 0;
			powerManager->doSuspend(1);
		}
	}

	while (input->isActive(SETTINGS)) { // SETTINGS HOLD
		wasActive = SETTINGS;

		input->update();

		if (SDL_GetTicks() - button_hold > 1000) {
			wasActive = SCREENSHOT;
			break;
		}
	}

	while (input->isActive(MENU)) { // MENU HOLD
		wasActive = MENU;

		input->update();

		if (input->isActive(SETTINGS)) {
			wasActive = POWER;
		} else if (input->isActive(BACKLIGHT_HOTKEY)) {
			wasActive = BACKLIGHT;
		} else if (input->isActive(VOLUME_HOTKEY)) {
			wasActive = VOLUP;
		} else if (input->isActive(POWER)) {
			wasActive = UDC_CONNECT;
		} else if (input->isActive(CONFIRM) || input->isActive(CANCEL)) {
			wasActive = 0;
			break;
		} else {
			continue;
		}
		break;
	}

	input->setActive(wasActive);

	if (input->isActive(POWER)) {
		// powerManager->doSuspend(1);
		poweroffDialog();

	} else if (input->isActive(SCREENSHOT)) {
		if (!saveScreenshot(confStr["homePath"])) {
			ERROR("Can't save screenshot");
			return true;
		}
		MessageBox mb(this, tr["Screenshot saved"]);
		mb.setAutoHide(1000);
		mb.exec();

	} else if (input->isActive(VOLUP) || input->isActive(VOLDOWN)) {
		setVolume(confInt["globalVolume"], true);

	} else if (input->isActive(BACKLIGHT)) {
		setBacklight(confInt["backlight"], true);

	} else if (input->isActive(UDC_CONNECT)) {
		powerManager->setPowerTimeout(0);
		input->battery = 6;
		platform->setUDC(UDC_CONNECT);

	} else if (input->isActive(UDC_REMOVE)) {
		platform->setUDC(UDC_REMOVE);
		inetIcon = "";
		input->battery = platform->getBattery();
		powerManager->setPowerTimeout(confInt["powerTimeout"]);

	} else if (input->isActive(TV_CONNECT)) {
		platform->setTVOut(-1);

	} else if (input->isActive(TV_REMOVE)) {
		platform->setTVOut(TV_OFF);

	} else if (input->isActive(JOYSTICK_CONNECT)) {
		input->initJoysticks(true);

	// } else if (input->isActive(PHONES_CONNECT)) {
	// 	// tvOutDialog(TV_OFF);
	// 	WARNING("volume mode changed");
	// 	return true;

	} else if (input->isActive(MMC_INSERT) || input->isActive(MMC_REMOVE)) {
		confInt["section"] = menu->getSectionIndex();
		confInt["link"] = menu->getLinkIndex();
		initMenu();

	} else {
		return false;
	}
	return true;
}

void GMenu2X::cls(Surface *s, bool flip) {
	if (s == NULL) {
		s = this->s;
	}

	s->box((SDL_Rect){0, 0, s->width(), s->height()}, (RGBAColor){0, 0, 0, 255});

	if (flip) {
		s->flip();
	}
}

string GMenu2X::setBackground(Surface *bg, string wallpaper) {
	if (!sc.exists(wallpaper)) { // search and scale background
		if (wallpaper.empty() || sc[wallpaper] == NULL) {
			DEBUG("Searching wallpaper");
			FileLister fl;
			fl.showDirectories = false;
			fl.showFiles = true;
			fl.setFilter(".png,.jpg,.jpeg,.bmp");
			fl.browse(data_path("skins/Default/wallpapers"));
			wallpaper = data_path("skins/Default/wallpapers/") + fl.getFiles()[0];
		}
		if (sc[wallpaper] == NULL) return "";
		if (confStr["bgscale"] == "Stretch") sc[wallpaper]->softStretch(platform->w, platform->h, SScaleStretch);
		else if (confStr["bgscale"] == "Crop") sc[wallpaper]->softStretch(platform->w, platform->h, SScaleMax);
		else if (confStr["bgscale"] == "Aspect") sc[wallpaper]->softStretch(platform->w, platform->h, SScaleFit);
	}

	cls(bg, false);
	sc[wallpaper]->blit(bg, (platform->w - sc[wallpaper]->width()) / 2, (platform->h - sc[wallpaper]->height()) / 2);
	return wallpaper;
}

void GMenu2X::initFont() {
	string skinFont = confStr["skinFont"] == "Default" ? data_path("skins/Default/font.ttf") : sc.getSkinFilePath("font.ttf");

	delete font;
	font = new FontHelper(skinFont, skinConfInt["fontSize"], skinConfColor["font"], skinConfColor["fontOutline"]);
	if (!font->font) {
		delete font;
		font = new FontHelper(data_path("skins/Default/font.ttf"), skinConfInt["fontSize"], skinConfColor["font"], skinConfColor["fontOutline"]);
	}

	delete titlefont;
	titlefont = new FontHelper(skinFont, skinConfInt["fontSizeTitle"], skinConfColor["font"], skinConfColor["fontOutline"]);
	if (!titlefont->font) {
		delete titlefont;
		titlefont = new FontHelper(data_path("skins/Default/font.ttf"), skinConfInt["fontSizeTitle"], skinConfColor["font"], skinConfColor["fontOutline"]);
	}
}

void GMenu2X::initMenu() {
	// Menu structure handler
	menu->initLayout();
	menu->readSections();
	menu->readLinks();

	for (uint32_t i = 0; i < menu->getSections().size(); i++) {
		// Add virtual links in the applications section
		if (menu->getSections()[i] == "applications") {
			menu->addActionLink(i, "Explorer", MakeDelegate(this, &GMenu2X::explorer), "Browse files and launch apps", "explorer.png");
			if (platform->ext_sd) {
				menu->addActionLink(i, "Umount", MakeDelegate(this, &GMenu2X::umountSdDialog), "Umount external media device", "eject.png");
			}
		}
		// Add virtual links in the setting section
		else if (menu->getSections()[i] == "settings") {
			menu->addActionLink(i, "Settings", MakeDelegate(this, &GMenu2X::settings), "Configure system", "configure.png");
			menu->addActionLink(i, "Skin", MakeDelegate(this, &GMenu2X::skinMenu), "Appearance & skin settings", "skin.png");
			if (file_exists(home_path("log.txt"))) {
				menu->addActionLink(i, "Log Viewer", MakeDelegate(this, &GMenu2X::viewLog), "Displays last launched program's output", "ebook.png");
			}

			menu->addActionLink(i, "About", MakeDelegate(this, &GMenu2X::about), "Info about GMenuNX", "about.png");
			menu->addActionLink(i, "Power", MakeDelegate(this, &GMenu2X::poweroffDialog), "Power menu", "exit.png");
		}
	}
	menu->setSectionIndex(confInt["section"]);
	menu->setLinkIndex(confInt["link"]);
}

void GMenu2X::settings() {
	powerManager->clearTimer();

	string lang = tr.getLang();

	vector<string> opFactory;
	opFactory.push_back(">>");
	string tmp = ">>";

	SettingsDialog sd(this, /*ts,*/ tr["Settings"], "skin:icons/configure.png");
	sd.allowCancel = false;

	int prevgamma = confInt["gamma"];

	sd.addSetting(new MenuSettingMultiString(this, tr["Language"], tr["Set the language used by GMenuNX"], &lang, tr.getLanguages()));

	if (platform->rtc) {
		prevDateTime = get_date_time();
		newDateTime = prevDateTime;
		sd.addSetting(new MenuSettingDateTime(this, tr["Date & Time"], tr["Set system's date & time"], &newDateTime));
	}

	sd.addSetting(new MenuSettingDir(this, tr["Home path"],	tr["Set as home for launched links"], &confStr["homePath"]));

	if (platform->udc) {
		vector<string> usbMode;
		usbMode.push_back("Ask");
		usbMode.push_back("Storage");
		usbMode.push_back("Charger");
		sd.addSetting(new MenuSettingMultiString(this, tr["USB mode"], tr["Define default USB mode"], &confStr["usbMode"], usbMode));
	}

	if (platform->tvout) {
		vector<string> tvMode;
		tvMode.push_back("Ask");
		tvMode.push_back("NTSC");
		tvMode.push_back("PAL");
		tvMode.push_back("OFF");
		sd.addSetting(new MenuSettingMultiString(this, tr["TV mode"], tr["Define default TV mode"], &confStr["tvMode"], tvMode));
	}

	sd.addSetting(new MenuSettingInt(this, tr["Suspend timeout"], tr["Seconds until suspend the device when inactive"], &confInt["backlightTimeout"], 30, 10, 300));
	sd.addSetting(new MenuSettingInt(this, tr["Power timeout"], tr["Minutes to poweroff system if inactive"], &confInt["powerTimeout"], 10, 1, 300));
	sd.addSetting(new MenuSettingInt(this, tr["Backlight"], tr["Set LCD backlight"], &confInt["backlight"], 70, 1, 100, 1, MakeDelegate(this, &GMenu2X::updateBacklightSetting)));
	sd.addSetting(new MenuSettingInt(this, tr["Audio volume"], tr["Set the default audio volume"], &confInt["globalVolume"], 60, 0, 100));
	sd.addSetting(new MenuSettingBool(this, tr["Remember selection"], tr["Remember the last selected section, link and file"], &confInt["saveSelection"]));
	sd.addSetting(new MenuSettingBool(this, tr["Output logs"], tr["Logs the link's output to read with Log Viewer"], &confInt["outputLogs"]));
	sd.addSetting(new MenuSettingMultiString(this, tr["Reset settings"], tr["Choose settings to reset back to defaults"], &tmp, opFactory, 0, MakeDelegate(this, &GMenu2X::resetSettings)));

	if (sd.exec() && sd.edited() && sd.save) {
		if (confStr["lang"] != lang) {
			confStr["lang"] = lang;
			tr.setLang(lang);
		}

		setBacklight(confInt["backlight"], false);
		writeConfig();

		if (prevgamma != confInt["gamma"]) {
			platform->setGamma(confInt["gamma"]);
		}

		if (prevDateTime != newDateTime) {
			set_date_time(newDateTime.c_str());
			quit();
		}
	}
	powerManager->setSuspendTimeout(confInt["backlightTimeout"]);
	powerManager->setPowerTimeout(confInt["powerTimeout"]);
}

void GMenu2X::resetSettings() {
	bool
		reset_gmenu = true,
		reset_skin = true,
		reset_icon = false,
		/* reset_homedir = false, */
		reset_manual = false,
		reset_parameter = false,
		reset_backdrop = false,
		reset_filter = false,
		reset_directory = false,
		reset_boxart = false,
		reset_cpu = false;

	SettingsDialog sd(this, /*ts,*/ tr["Reset settings"], "skin:icons/configure.png");
	sd.addSetting(new MenuSettingBool(this, tr["GMenuNX"], tr["Reset GMenuNX settings"], &reset_gmenu));
	sd.addSetting(new MenuSettingBool(this, tr["Default skin"], tr["Reset Default skin settings back to default"], &reset_skin));
	sd.addSetting(new MenuSettingBool(this, tr["Icons"], tr["Reset link's icon back to default"], &reset_icon));
	sd.addSetting(new MenuSettingBool(this, tr["Manuals"], tr["Unset link's manual"], &reset_manual));
	// sd.addSetting(new MenuSettingBool(this, tr["Home directory"], tr["Unset link's home directory"], &reset_homedir));
	sd.addSetting(new MenuSettingBool(this, tr["Parameters"], tr["Unset link's additional parameters"], &reset_parameter));
	sd.addSetting(new MenuSettingBool(this, tr["Backdrops"], tr["Unset link's backdrops"], &reset_backdrop));
	sd.addSetting(new MenuSettingBool(this, tr["Filters"], tr["Unset link's selector file filters"], &reset_filter));
	sd.addSetting(new MenuSettingBool(this, tr["Directories"], tr["Unset link's selector directory"], &reset_directory));
	sd.addSetting(new MenuSettingBool(this, tr["Box art"], tr["Unset link's selector box art path"], &reset_boxart));

	if (platform->cpu_max != platform->cpu_min) {
		sd.addSetting(new MenuSettingBool(this, tr["CPU speed"], tr["Reset link's custom CPU speed back to default"], &reset_cpu));
	}

	if (sd.exec() && sd.edited() && sd.save) {
		MessageBox mb(this, tr["Changes will be applied to ALL\napps and GMenuNX. Are you sure?"], "skin:icons/exit.png");
		mb.setButton(CANCEL, tr["Cancel"]);
		mb.setButton(MANUAL,  tr["Yes"]);
		if (mb.exec() != MANUAL) return;

		for (uint32_t s = 0; s < menu->getSections().size(); s++) {
			for (uint32_t l = 0; l < menu->sectionLinks(s)->size(); l++) {
				menu->setSectionIndex(s);
				menu->setLinkIndex(l);
				bool islink = menu->getLinkApp() != NULL;
				if (!islink) continue;
				if (reset_cpu)			menu->getLinkApp()->setCPU();
				if (reset_icon)			menu->getLinkApp()->setIcon("");
				// if (reset_homedir)		menu->getLinkApp()->setHomeDir("");
				if (reset_manual)		menu->getLinkApp()->setManual("");
				if (reset_parameter) 	menu->getLinkApp()->setParams("");
				if (reset_filter) 		menu->getLinkApp()->setSelectorFilter("");
				if (reset_directory) 	menu->getLinkApp()->setSelectorDir("");
				if (reset_boxart) 		menu->getLinkApp()->setSelectorScreens("");
				if (reset_backdrop) 	menu->getLinkApp()->setBackdrop("");
				if (reset_icon || reset_manual || reset_parameter || reset_backdrop || reset_filter || reset_directory || reset_boxart )
					menu->getLinkApp()->save();
			}
		}
		if (reset_skin) {
			unlink(home_path("skins/Default/skin.conf").c_str());
		}
		if (reset_gmenu) {
			unlink(home_path("gmenunx.conf").c_str());
		}
		quit();
	}
}

// void GMenu2X::cpuSettings() {
// 	SettingsDialog sd(this, ts, tr["CPU settings"], "skin:icons/configure.png");
// 	sd.addSetting(new MenuSettingInt(this, tr["Default CPU clock"], tr["Set the default working CPU frequency"], &confInt["cpuMenu"], 528, 528, 600, 6));
// 	sd.addSetting(new MenuSettingInt(this, tr["Maximum CPU clock"], tr["Maximum overclock for launching links"], &confInt["cpuMax"], 740, 600, 1200, 6));
// 	// sd.addSetting(new MenuSettingInt(this, tr["Minimum CPU clock"], tr["Minimum underclock used in Suspend mode"], &confInt["cpuMin"], 342, 200, 528, 6));

// 	if (sd.exec() && sd.edited() && sd.save) {
// 		setCPU(confInt["cpuMenu"]);
// 		writeConfig();
// 	}
// }

bool GMenu2X::readTmp() {
	ifstream f("/tmp/gmenunx.tmp", std::ios_base::in);
	if (!f.is_open()) return false;

	string line;

	while (getline(f, line, '\n')) {
		string::size_type pos = line.find("=");
		string name = trim(line.substr(0, pos));
		string value = trim(line.substr(pos + 1));

		if (name == "section") menu->setSectionIndex(atoi(value.c_str()));
		else if (name == "link") menu->setLinkIndex(atoi(value.c_str()));
		else if (name == "selectorElement") confInt["selectorElement"] = atoi(value.c_str());
		else if (name == "selectorDir") confStr["selectorDir"] = value;
		// else if (name == "TVOut") TVOut = atoi(value.c_str());
		else if (name == "tvout") input->tvout_ = atoi(value.c_str());
		else if (name == "udc") input->udc_ = atoi(value.c_str());
		else if (name == "currBackdrop") currBackdrop = value;
		else if (name == "explorerLastDir") confStr["explorerLastDir"] = value;
	}
	// if (TVOut > 2) TVOut = 0;
	f.close();
	unlink("/tmp/gmenunx.tmp");
	return true;
}

void GMenu2X::writeTmp() {
	ofstream f("/tmp/gmenunx.tmp");
	if (!f.is_open()) return;

	f << "section=" << menu->getSectionIndex() << std::endl;
	f << "link=" << menu->getLinkIndex() << std::endl;
	f << "selectorElement=" << confInt["selectorElement"] << std::endl;
	f << "selectorDir=" << confStr["selectorDir"] << std::endl;
	f << "udc=" << input->udc << std::endl;
	f << "tvout=" << input->tvout << std::endl;
	// f << "TVOut=" << TVOut << std::endl;
	f << "currBackdrop=" << currBackdrop << std::endl;
	if (!confStr["explorerLastDir"].empty()) f << "explorerLastDir=" << confStr["explorerLastDir"] << std::endl;
	f.close();
}

void GMenu2X::readConfig(string conffile, bool defaults) {
	if (defaults) {
		// Defaults *** Sync with default values in writeConfig
		confInt["saveSelection"] = 1;
		confInt["section"] = 0;
		confInt["link"] = 0;
		confInt["skinBackdrops"] = 0;
		confStr["homePath"] = home_path("../");
		confInt["globalVolume"] = 60;
		confStr["bgscale"] = "Crop";
		confStr["skinFont"] = "Skin";
	}

	ifstream f(conffile, std::ios_base::in);
	if (f.is_open()) {
		string line;
		while (getline(f, line, '\n')) {
			string::size_type pos = line.find("=");
			string name = trim(line.substr(0, pos));
			string value = trim(line.substr(pos + 1));

			if (value.length() > 1 && value.at(0) == '"' && value.at(value.length() - 1) == '"')
				confStr[name] = value.substr(1, value.length() - 2);
			else
				confInt[name] = atoi(value.c_str());
		}
		f.close();
	}

	tr.setLang(confStr["lang"]);
	if (!confStr["wallpaper"].empty() && !file_exists(confStr["wallpaper"])) confStr["wallpaper"] = "";
	if (confStr["skin"].empty() || !dir_exists(confStr["skin"])) confStr["skin"] = data_path("skins/Default");

	evalIntConf(&confInt["backlightTimeout"], 30, 10, 300);
	evalIntConf(&confInt["powerTimeout"], 10, 1, 300);
	evalIntConf(&confInt["outputLogs"], 0, 0, 1 );
	// evalIntConf(&confInt["cpuMax"], 2000, 200, 2000 );
	// evalIntConf(&confInt["cpuMin"], 342, 200, 1200 );
	// evalIntConf(&confInt["cpuMenu"], 528, 200, 1200 );
	evalIntConf(&confInt["globalVolume"], 60, 0, 100 );
	evalIntConf(&confInt["backlight"], 70, 1, 100);
	evalIntConf(&confInt["minBattery"], 3550, 1, 10000);
	evalIntConf(&confInt["maxBattery"], 3720, 1, 10000);
}

void GMenu2X::writeConfig() {
	if (confInt["saveSelection"] && menu != NULL) {
		confInt["section"] = menu->getSectionIndex();
		confInt["link"] = menu->getLinkIndex();
	} else {
		confInt["selectorElement"] = -1;
		confStr["selectorDir"] = "";
	}

	ofstream f(home_path("gmenunx.conf"));
	if (!f.is_open()) return;

	for (ConfStrHash::iterator curr = confStr.begin(); curr != confStr.end(); curr++) {
		if (
			// deprecated
			curr->first == "sectionBarPosition" ||
			curr->first == "tvoutEncoding" ||
			curr->first == "datetime" ||
			curr->first == "explorerLastDir" ||
			curr->first == "defaultDir" ||

			// defaults
			(curr->first == "homePath" && curr->second == home_path("../")) ||
			(curr->first == "skin" && curr->second == "Default") ||
			(curr->first == "previewMode" && curr->second == "Miniature") ||
			(curr->first == "skinFont" && curr->second == "Skin") ||
			(curr->first == "usbMode" && curr->second == "Ask") ||
			(curr->first == "tvMode" && curr->second == "Ask") ||
			(curr->first == "lang" && curr->second.empty()) ||
			(curr->first == "lang" && curr->second == "English") ||
			(curr->first == "bgscale" && curr->second.empty()) ||
			(curr->first == "bgscale" && curr->second == "Crop") ||

			curr->first.empty() || curr->second.empty()
		) continue;

		f << curr->first << "=\"" << curr->second << "\"" << std::endl;
	}

	for (ConfIntHash::iterator curr = confInt.begin(); curr != confInt.end(); curr++) {
		if (
			// deprecated
			curr->first == "batteryLog" ||
			curr->first == "maxClock" ||
			curr->first == "minClock" ||
			curr->first == "menuClock" ||
			curr->first == "TVOut" ||
			curr->first == "cpuLink" ||
			curr->first == "cpuMenu" ||
			curr->first == "cpuMax" ||
			curr->first == "cpuMin" ||

			// moved to skin conf
			curr->first == "linkCols" ||
			curr->first == "linkRows" ||
			curr->first == "sectionBar" ||
			curr->first == "sectionLabel" ||
			curr->first == "linkLabel" ||

			// defaults
			(curr->first == "skinBackdrops" && curr->second == 0) ||
			(curr->first == "backlightTimeout" && curr->second == 30) ||
			(curr->first == "powerTimeout" && curr->second == 10) ||
			(curr->first == "outputLogs" && curr->second == 0) ||
			// (curr->first == "cpuMin" && curr->second == 342) ||
			// (curr->first == "cpuMax" && curr->second == 740) ||
			// (curr->first == "cpuMenu" && curr->second == 528) ||
			(curr->first == "globalVolume" && curr->second == 60) ||
			(curr->first == "backlight" && curr->second == 70) ||
			(curr->first == "minBattery" && curr->second == 3550) ||
			(curr->first == "maxBattery" && curr->second == 3720) ||
			(curr->first == "saveSelection" && curr->second == 1) ||
			(curr->first == "section" && curr->second == 0) ||
			(curr->first == "link" && curr->second == 0) ||
			(curr->first == "selectorElement" && curr->second == -1) ||

			curr->first.empty()
		) continue;

		f << curr->first << "=" << curr->second << std::endl;
	}
	f.close();
	sync();
}

void GMenu2X::writeSkinConfig() {
	if (confStr["skin"] == data_path("skins/Default")) {
		mkdir(home_path("skins").c_str(), 0777);
		mkdir(home_path("skins/Default").c_str(), 0777);
		confStr["skin"] = home_path("skins/Default");
	}

	ofstream f(confStr["skin"] + "/skin.conf");
	if (!f.is_open()) return;

	for (ConfStrHash::iterator curr = skinConfStr.begin(); curr != skinConfStr.end(); curr++) {
		if (curr->first.empty() || curr->second.empty()) {
			continue;
		}

		f << curr->first << "=\"" << curr->second << "\"" << std::endl;
	}

	for (ConfIntHash::iterator curr = skinConfInt.begin(); curr != skinConfInt.end(); curr++) {
		if (
			curr->first == "titleFontSize" ||
			curr->first == "sectionBarHeight" ||
			curr->first == "linkHeight" ||
			curr->first == "selectorPreviewX" ||
			curr->first == "selectorPreviewY" ||
			curr->first == "selectorPreviewWidth" ||
			curr->first == "selectorPreviewHeight" ||
			curr->first == "selectorX" ||
			curr->first == "linkItemHeight" ||
			curr->first == "topBarHeight" ||

			(curr->first == "previewWidth" && curr->second == 128) ||
			(curr->first == "linkCols" && curr->second == 4) ||
			(curr->first == "linkRows" && curr->second == 4) ||
			(curr->first == "sectionBar" && curr->second == SB_CLASSIC) ||
			(curr->first == "sectionLabel" && curr->second == 1) ||
			(curr->first == "linkLabel" && curr->second == 1) ||
			(curr->first == "showDialogIcon" && curr->second == 1) ||

			curr->first.empty()
		) {
			continue;
		}

		f << curr->first << "=" << curr->second << std::endl;
	}

	vector<std::string> valid {
		"topBarBg",
		"listBg",
		"bottomBarBg",
		"selectionBg",
		"previewBg",
		"messageBoxBg",
		"messageBoxBorder",
		"messageBoxSelection",
		"font",
		"fontOutline",
		"fontAlt",
		"fontAltOutline"
	};

	for (ConfColorHash::iterator curr = skinConfColor.begin(); curr != skinConfColor.end(); curr++) {
		if (count(valid.begin(), valid.end(), curr->first)) {
			f << curr->first << "=" << rgbatostr(curr->second) << std::endl;
		}
	}

	f.close();

	sync();
}

void GMenu2X::setSkin(string skin, bool clearSC) {
	input->update(false);
	if (input->isActive(SETTINGS)) skin = data_path("skins/Default");

	confStr["skin"] = skin;

	// Clear previous skin settings
	skinConfStr.clear();
	skinConfInt.clear();

	// Defaults *** Sync with default values in writeConfig
	skinConfInt["sectionBar"] = SB_CLASSIC;
	skinConfInt["sectionLabel"] = 1;
	skinConfInt["linkLabel"] = 1;
	skinConfInt["showDialogIcon"] = 1;

	// clear collection and change the skin path
	if (clearSC) {
		sc.clear();
	}

	sc.setSkin(skin);

	// reset colors to the default values
	skinConfColor["topBarBg"] = (RGBAColor){44, 129, 213, 255};
	skinConfColor["listBg"] = (RGBAColor){0, 0, 48, 60};
	skinConfColor["bottomBarBg"] = (RGBAColor){44, 129, 213, 255};
	skinConfColor["selectionBg"] = (RGBAColor){255, 255, 255, 204};
	skinConfColor["previewBg"] = (RGBAColor){253, 1, 252, 0};
	skinConfColor["messageBoxBg"] = (RGBAColor){255, 255, 255, 255};
	skinConfColor["messageBoxBorder"] = (RGBAColor){80, 80, 80, 255};
	skinConfColor["messageBoxSelection"] = (RGBAColor){160, 160, 160, 255};
	skinConfColor["font"] = (RGBAColor){255, 255, 255, 255};
	skinConfColor["fontOutline"] = (RGBAColor){0, 0, 0, 192};
	skinConfColor["fontAlt"] = (RGBAColor){253, 1, 252, 0};
	skinConfColor["fontAltOutline"] = (RGBAColor){253, 1, 252, 0};

	// load skin settings
	skin = skin + "/skin.conf";

	ifstream f(skin, std::ios_base::in);
	if (!f.is_open()) {
		INFO("File not found: %s. Using default values.", skin.c_str());
	} else {
		string line;
		while (getline(f, line, '\n')) {
			line = trim(line);
			string::size_type pos = line.find("=");
			string name = trim(line.substr(0, pos));
			string value = trim(line.substr(pos + 1));

			if (value.length() > 0) {
				if (value.length() > 1 && value.at(0) == '"' && value.at(value.length() - 1) == '"') {
					skinConfStr[name] = value.substr(1, value.length() - 2);
				} else if (value.at(0) == '#') {
					skinConfColor[name] = strtorgba(value);
				} else if (name.length() > 6 && name.substr(name.length() - 6, 5) == "Color") {
					value += name.substr(name.length() - 1);
					name = name.substr(0, name.length() - 6);
					if (name == "selection" || name == "topBar" || name == "bottomBar" || name == "messageBox") name += "Bg";
					if (value.substr(value.length() - 1) == "R") skinConfColor[name].r = atoi(value.c_str());
					if (value.substr(value.length() - 1) == "G") skinConfColor[name].g = atoi(value.c_str());
					if (value.substr(value.length() - 1) == "B") skinConfColor[name].b = atoi(value.c_str());
					if (value.substr(value.length() - 1) == "A") skinConfColor[name].a = atoi(value.c_str());
				} else {
					skinConfInt[name] = atoi(value.c_str());
				}
			}
		}
		f.close();
	}

	// (poor) HACK: ensure some colors have a default value
	if (skinConfColor["fontAlt"].r == 253 && skinConfColor["fontAlt"].g == 1 && skinConfColor["fontAlt"].b == 252 && skinConfColor["fontAlt"].a == 0) {
		skinConfColor["fontAlt"] = skinConfColor["font"];
	}
	if (skinConfColor["fontAltOutline"].r == 253 && skinConfColor["fontAltOutline"].g == 1 && skinConfColor["fontAltOutline"].b == 252 && skinConfColor["fontAltOutline"].a == 0) {
		skinConfColor["fontAltOutline"] = skinConfColor["fontOutline"];
	}
	if (skinConfColor["previewBg"].r == 253 && skinConfColor["previewBg"].g == 1 && skinConfColor["previewBg"].b == 252 && skinConfColor["previewBg"].a == 0) {
		skinConfColor["previewBg"] = skinConfColor["topBarBg"];
		skinConfColor["previewBg"].a == 170;
	}

	// prevents breaking current skin until they are updated
	if (!skinConfInt["fontSizeTitle"] && skinConfInt["titleFontSize"] > 0) skinConfInt["fontSizeTitle"] = skinConfInt["titleFontSize"];

	evalIntConf(&skinConfInt["sectionBarSize"], 40, 1, platform->w);
	evalIntConf(&skinConfInt["bottomBarHeight"], 16, 1, platform->h);
	evalIntConf(&skinConfInt["previewWidth"], 128, 1, platform->w);
	evalIntConf(&skinConfInt["fontSize"], 12, 6, 60);
	evalIntConf(&skinConfInt["fontSizeTitle"], 20, 6, 60);
	evalIntConf(&skinConfInt["sectionBar"], SB_CLASSIC, 0, 5);
	evalIntConf(&skinConfInt["linkCols"], 4, 1, 8);
	evalIntConf(&skinConfInt["linkRows"], 4, 1, 8);

	initFont();
}

void GMenu2X::skinMenu() {
	bool save = false;
	int selected = 0;
	string initSkin = confStr["skin"];
	string prevSkin = "/";

	vector<string> skins;

	FileLister fl;
	fl.showFullPath = true;
	fl.showDirectories = true;
	fl.showFiles = false;
	fl.addExclude("..");
	fl.addExclude("Default");
	fl.browse(data_path("skins"));
	skins = fl.getDirectories();
	fl.browse(home_path("skins"));
	skins.insert(skins.end(), fl.getDirectories().begin(), fl.getDirectories().end());
	skins.insert(skins.begin(), data_path("skins/Default"));

	vector<string> wpLabel;
	wpLabel.push_back(">>");
	string tmp = ">>";

	vector<string> sbStr;
	sbStr.push_back("OFF");
	sbStr.push_back("Left");
	sbStr.push_back("Bottom");
	sbStr.push_back("Right");
	sbStr.push_back("Top");
	sbStr.push_back("Classic");
	int sbPrev = skinConfInt["sectionBar"];
	string sectionBar = sbStr[skinConfInt["sectionBar"]];

	int linkColsPrev = skinConfInt["linkCols"];
	int linkRowsPrev = skinConfInt["linkRows"];

	vector<string> bgScale;
	bgScale.push_back("Original");
	bgScale.push_back("Crop");
	bgScale.push_back("Aspect");
	bgScale.push_back("Stretch");
	string bgScalePrev = confStr["bgscale"];

	vector<string> bdStr;
	bdStr.push_back("OFF");
	bdStr.push_back("Menu only");
	bdStr.push_back("Dialog only");
	bdStr.push_back("Menu & Dialog");
	int bdPrev = confInt["skinBackdrops"];
	string skinBackdrops = bdStr[confInt["skinBackdrops"]];

	vector<string> skinFont;
	skinFont.push_back("Skin");
	skinFont.push_back("Default");
	string skinFontPrev = confStr["skinFont"];

	vector<string> previewMode;
	previewMode.push_back("Miniature");
	previewMode.push_back("Backdrop");

	vector<string> wallpapers;

	fl.showDirectories = false;
	fl.showFiles = true;
	fl.setFilter(".png,.jpg,.jpeg,.bmp");

	do {
		if (prevSkin != confStr["skin"] || skinFontPrev != confStr["skinFont"]) {
			prevSkin = confStr["skin"];
			skinFontPrev = confStr["skinFont"];

			setSkin(confStr["skin"], false);
			sectionBar = sbStr[skinConfInt["sectionBar"]];

			wallpapers.clear();

			if (confStr["skin"] != data_path("skins/Default")) {
				fl.browse(confStr["skin"] + "/wallpapers");
				wallpapers.insert(wallpapers.end(), fl.getFiles().begin(), fl.getFiles().end());
			}

			fl.browse(data_path("skins/Default/wallpapers"));
			wallpapers.insert(wallpapers.end(), fl.getFiles().begin(), fl.getFiles().end());

			sc.del("skin:icons/skin.png");
			sc.del("skin:imgs/buttons/left.png");
			sc.del("skin:imgs/buttons/right.png");
			sc.del("skin:imgs/buttons/a.png");

			confStr["wallpaper"] = wallpapers.at(0);
		}

		sc.del(confStr["wallpaper"]);
		setBackground(bg, confStr["wallpaper"]);

		SettingsDialog sd(this, /*ts,*/ tr["Skin"], "skin:icons/skin.png");
		sd.selected = selected;
		sd.allowCancel = false;
		sd.addSetting(new MenuSettingMultiString(this, tr["Skin"], tr["Set the skin used by GMenuNX"], &confStr["skin"], skins, MakeDelegate(this, &GMenu2X::updateSkin), MakeDelegate(this, &GMenu2X::changeSkin), MakeDelegate(this, &GMenu2X::basenameFormatter)));
		sd.addSetting(new MenuSettingMultiString(this, tr["Wallpaper"], tr["Select an image to use as a wallpaper"], &confStr["wallpaper"], wallpapers, MakeDelegate(this, &GMenu2X::updateSkin), MakeDelegate(this, &GMenu2X::changeWallpaper), MakeDelegate(this, &GMenu2X::basenameFormatter)));
		sd.addSetting(new MenuSettingMultiString(this, tr["Background"], tr["How to scale wallpaper, backdrops and game art"], &confStr["bgscale"], bgScale, MakeDelegate(this, &GMenu2X::updateSkin)));
		sd.addSetting(new MenuSettingMultiString(this, tr["Preview mode"], tr["How to show image preview and game art"], &confStr["previewMode"], previewMode));
		sd.addSetting(new MenuSettingMultiString(this, tr["Skin colors"], tr["Customize skin colors"], &tmp, wpLabel, MakeDelegate(this, &GMenu2X::updateSkin), MakeDelegate(this, &GMenu2X::skinColors)));
		sd.addSetting(new MenuSettingMultiString(this, tr["Skin backdrops"], tr["Automatic load backdrops from skin pack"], &skinBackdrops, bdStr));
		sd.addSetting(new MenuSettingMultiString(this, tr["Font face"], tr["Override the skin font face"], &confStr["skinFont"], skinFont, MakeDelegate(this, &GMenu2X::updateSkin)));
		sd.addSetting(new MenuSettingInt(this, tr["Font size"], tr["Size of text font"], &skinConfInt["fontSize"], 12, 6, 60));
		sd.addSetting(new MenuSettingInt(this, tr["Title font size"], tr["Size of title's text font"], &skinConfInt["fontSizeTitle"], 20, 6, 60));
		sd.addSetting(new MenuSettingMultiString(this, tr["Section bar layout"], tr["Set the layout and position of the Section Bar"], &sectionBar, sbStr));
		sd.addSetting(new MenuSettingInt(this, tr["Section bar size"], tr["Size of section and top bar"], &skinConfInt["sectionBarSize"], 40, 1, platform->w));
		sd.addSetting(new MenuSettingInt(this, tr["Bottom bar height"], tr["Height of bottom bar"], &skinConfInt["bottomBarHeight"], 16, 1, platform->h));
		sd.addSetting(new MenuSettingInt(this, tr["Menu columns"], tr["Number of columns of links in main menu"], &skinConfInt["linkCols"], 4, 1, 8));
		sd.addSetting(new MenuSettingInt(this, tr["Menu rows"], tr["Number of rows of links in main menu"], &skinConfInt["linkRows"], 4, 1, 8));
		sd.addSetting(new MenuSettingBool(this, tr["Link label"], tr["Show link labels in main menu"], &skinConfInt["linkLabel"]));
		sd.addSetting(new MenuSettingBool(this, tr["Section label"], tr["Show the active section label in main menu"], &skinConfInt["sectionLabel"]));
		sd.exec();

		selected = sd.selected;
		save = sd.save;
	} while (!save);

	if (skinBackdrops == "OFF") confInt["skinBackdrops"] = BD_OFF;
	else if (skinBackdrops == "Menu & Dialog") confInt["skinBackdrops"] = BD_MENU | BD_DIALOG;
	else if (skinBackdrops == "Menu only") confInt["skinBackdrops"] = BD_MENU;
	else if (skinBackdrops == "Dialog only") confInt["skinBackdrops"] = BD_DIALOG;

	if (sectionBar == "OFF") skinConfInt["sectionBar"] = SB_OFF;
	else if (sectionBar == "Right") skinConfInt["sectionBar"] = SB_RIGHT;
	else if (sectionBar == "Top") skinConfInt["sectionBar"] = SB_TOP;
	else if (sectionBar == "Bottom") skinConfInt["sectionBar"] = SB_BOTTOM;
	else if (sectionBar == "Left") skinConfInt["sectionBar"] = SB_LEFT;
	else skinConfInt["sectionBar"] = SB_CLASSIC;

	writeSkinConfig();
	writeConfig();

	if (
		bdPrev != confInt["skinBackdrops"] ||
		initSkin != confStr["skin"] ||
		bgScalePrev != confStr["bgscale"] ||
		linkColsPrev != skinConfInt["linkCols"] ||
		linkRowsPrev != skinConfInt["linkRows"] ||
		sbPrev != skinConfInt["sectionBar"]
	) quit();
}

void GMenu2X::skinColors() {
	bool save = false;
	do {
		setSkin(confStr["skin"], false);

		SettingsDialog sd(this, /*ts,*/ tr["Skin Colors"], "skin:icons/skin.png");
		sd.allowCancel = false;
		sd.addSetting(new MenuSettingRGBA(this, tr["Top/Section Bar"], tr["Color of the top and section bar"], &skinConfColor["topBarBg"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["List Body"], tr["Color of the list body"], &skinConfColor["listBg"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Bottom Bar"], tr["Color of the bottom bar"], &skinConfColor["bottomBarBg"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Selection"], tr["Color of the selection and other interface details"], &skinConfColor["selectionBg"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Box Art"], tr["Color of the box art background"], &skinConfColor["previewBg"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Message Box"], tr["Background color of the message box"], &skinConfColor["messageBoxBg"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Msg Box Border"], tr["Border color of the message box"], &skinConfColor["messageBoxBorder"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Msg Box Selection"], tr["Color of the selection of the message box"], &skinConfColor["messageBoxSelection"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Font"], tr["Color of the font"], &skinConfColor["font"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Font Outline"], tr["Color of the font's outline"], &skinConfColor["fontOutline"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Alt Font"], tr["Color of the alternative font"], &skinConfColor["fontAlt"]));
		sd.addSetting(new MenuSettingRGBA(this, tr["Alt Font Outline"], tr["Color of the alternative font outline"], &skinConfColor["fontAltOutline"]));
		sd.exec();
		save = sd.save;
	} while (!save);
	writeSkinConfig();
}

void GMenu2X::about() {
	vector<string> text;
	TextDialog td(this, "GMenuNX", tr["Build:"] + " " + __DATE__ + " " + __TIME__, "skin:icons/about.png");
	td.appendFile(data_path("about.txt"));
	td.exec();
}

void GMenu2X::viewLog() {
	string logfile = home_path("log.txt");
	if (!file_exists(logfile)) return;

	TextDialog td(this, tr["Log Viewer"], tr["Last launched program's output"], "skin:icons/ebook.png");
	td.appendFile(logfile);
	td.exec();

	MessageBox mb(this, tr["Delete the log file?"], "skin:icons/ebook.png");
	mb.setButton(MANUAL, tr["Delete and disable"]);
	mb.setButton(CONFIRM, tr["Delete"]);
	mb.setButton(CANCEL,  tr["No"]);
	int res = mb.exec();

	switch (res) {
		case MANUAL:
			confInt["outputLogs"] = false;

		case CONFIRM:
			unlink(logfile.c_str());
			sync();
			initMenu();
			break;
	}
}

void GMenu2X::changeWallpaper() {
	WallpaperDialog wp(this, tr["Wallpaper"], tr["Select an image to use as a wallpaper"], "skin:icons/wallpaper.png");
	if (wp.exec() && confStr["wallpaper"] != wp.wallpaper) {
		confStr["wallpaper"] = wp.wallpaper;
	}
}

void GMenu2X::changeSkin() {
	SkinDialog sd(this, tr["skin"], tr["Set the skin used by GMenuNX"], "skin:icons/skin.png");
	if (sd.exec() && confStr["skin"] != sd.skin) {
		confStr["skin"] = sd.skin;
	}
}

void GMenu2X::showManual() {
	string linkTitle = menu->getLinkApp()->getTitle();
	string linkDescription = menu->getLinkApp()->getDescription();
	string linkIcon = menu->getLinkApp()->getIcon();
	string linkManual = menu->getLinkApp()->getManualPath();
	string linkBackdrop = confInt["skinBackdrops"] | BD_DIALOG ? menu->getLinkApp()->getBackdropPath() : "";
	string linkExec = menu->getLinkApp()->getExec();

	if (linkManual == "") return;

	TextDialog td(this, linkTitle, linkDescription, linkIcon); //, linkBackdrop);

	if (file_exists(linkManual)) {
		string ext = linkManual.substr(linkManual.size() - 4, 4);
		if (ext == ".png" || ext == ".bmp" || ext == ".jpg" || ext == "jpeg") {
			ImageViewerDialog im(this, linkTitle, linkDescription, linkIcon, linkManual);
			im.exec();
			return;
		}

		td.appendFile(linkManual);
	} else if (!platform->opk.empty() && file_ext(linkExec, true) == ".opk") {
		void *buf; size_t len;
		struct OPK *opk = opk_open(linkExec.c_str());

		if (!opk) {
			ERROR("Unable to open OPK");
			return;
		}

		if (opk_extract_file(opk, linkManual.c_str(), &buf, &len) < 0) {
			ERROR("Unable to extract file: %s\n", linkManual.c_str());
			return;
		}

		opk_close(opk);

		char* str = strdup((const char*)buf);
		str[len] = 0;

		td.appendText(str);
	} else {
		return;
	}

	td.exec();
}

void GMenu2X::explorer() {
	BrowseDialog bd(this, tr["Explorer"], tr["Select a file or application"]);
	bd.showDirectories = true;
	bd.showFiles = true;

	while (bd.exec(confStr["explorerLastDir"])) {
		confStr["explorerLastDir"] = bd.getDir();

		string ext = bd.getExt(bd.selected);
		if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" || ext == ".bmp") {
			ImageViewerDialog im(this, tr["Image viewer"], bd.getFile(bd.selected), "icons/explorer.png", bd.getPath(bd.selected));
			im.exec();
		} else if (ext == ".txt" || ext == ".conf" || ext == ".me" || ext == ".md" || ext == ".xml" || ext == ".log" || ext == ".ini") {
			TextDialog td(this, tr["Text viewer"], bd.getFile(bd.selected), "skin:icons/ebook.png");
			td.appendFile(bd.getPath(bd.selected));
			td.exec();
		} else if (platform->ipk && ext == ".ipk" && file_exists("/usr/bin/opkg")) {
			ipkInstall(bd.getPath(bd.selected));
		} else if (!platform->opk.empty() && ext == ".opk") {
			opkInstall(bd.getPath(bd.selected));
		} else if (ext == ".sh") {
			TerminalDialog td(this, tr["Terminal"], "sh " + cmdclean(bd.getFileName(bd.selected)), "skin:icons/terminal.png");
			td.exec(bd.getPath(bd.selected));
		} else if (ext == ".zip" && file_exists("/usr/bin/unzip") && (bd.getFile(bd.selected).rfind("gmenu2x-skin-", 0) == 0) || (bd.getFile(bd.selected).rfind("gmenunx-skin-", 0) == 0)) {
			string path = bd.getPath(bd.selected);
			string skinname = base_name(path, true).substr(13); // strip gmenu2x-skin- and .zip
			if (skinname.size() > 1) {
				TerminalDialog td(this, tr["Skin installer"], tr["Installing skin %s"], "skin:icons/skin.png");
				string cmd = "rm -rf \"" + home_path("skins/") + skinname + "/\"; mkdir -p \"" + home_path("skins/") + skinname + "/\"; unzip \"" + path + "\" -d \"" + home_path("skins/") + skinname + "/\"; if [ `ls \"" + home_path("skins/") + skinname + "\" | wc -l` == 1 ]; then subdir=`ls \"" + home_path("skins/") + skinname + "\"`; mv \"" + home_path("skins/") + skinname + "\"/$subdir/* \"" + home_path("skins/") + skinname + "/\"; rmdir \"" + home_path("skins/") + skinname + "\"/$subdir; fi; sync";
				td.exec(cmd);
				setSkin(skinname, false);
			}
		} else if (ext == ".zip" && file_exists("/usr/bin/unzip")) {
			TerminalDialog td(this, tr["Zip content"], bd.getFileName(bd.selected), "skin:icons/terminal.png");
			td.exec("unzip -l " + cmdclean(bd.getPath(bd.selected)));
		} else {
			string command = cmdclean(bd.getPath(bd.selected));
			string params = "";

			if (confInt["outputLogs"]) {
				if (file_exists("/usr/bin/gdb")) {
					params = "-batch -ex \"run\" -ex \"bt\" --args " + command;
					command = "gdb";
				}
				params += " 2>&1 | tee " + cmdclean(home_path("log.txt"));
			}

			LinkApp *link = new LinkApp(this, "explorer.lnk~");
			link->setExec(command);
			link->setParams(params);
			link->setIcon("skin:icons/terminal.png");
			link->setTitle(bd.getFile(bd.selected));
			link->launch();
			return;
		}
	}
}

bool GMenu2X::saveScreenshot(string path) {
	if (file_exists("/usr/bin/fbgrab")) {
		path = unique_filename(path + "/screenshot", ".png");
		path = "/usr/bin/fbgrab " + path;
		return system(path.c_str()) == 0;
	}

	path = unique_filename(path + "/screenshot", ".bmp");
	return SDL_SaveBMP(s->raw, path.c_str()) == 0;
}

void GMenu2X::poweroffDialog() {
	MessageBox mb(this, tr["Poweroff or reboot the device?"], "skin:icons/exit.png");
	mb.setButton(SECTION_NEXT, tr["Reboot"]);
	mb.setButton(CONFIRM, tr["Poweroff"]);
	mb.setButton(CANCEL,  tr["Cancel"]);
	int res = mb.exec();
	switch (res) {
		case CONFIRM: {
			MessageBox mb(this, tr["Poweroff"]);
			mb.setAutoHide(1);
			mb.exec();
			setVolume(0);
			quit(false);
			execlp("/sbin/poweroff", "/sbin/poweroff", NULL);
			break;
		}
		case SECTION_NEXT: {
			MessageBox mb(this, tr["Rebooting"]);
			mb.setAutoHide(1);
			mb.exec();
			setVolume(0);
			quit(false);
			execlp("/sbin/reboot", "/sbin/reboot", NULL);
			break;
		}
	}
}

void GMenu2X::umountSdDialog() {
	BrowseDialog bd(this, tr["Umount"], tr["Umount external media device"], "skin:icons/eject.png");
	bd.showDirectories = true;
	bd.showFiles = false;
	bd.allowDirUp = false;
	bd.allowEnterDirectory = false;
	bd.exec("/media");
}

void GMenu2X::contextMenu() {
	vector<MenuOption> options;
	if (menu->getLinkApp() != NULL) {
		options.push_back((MenuOption){tr["Edit"] + " " + menu->getLink()->getTitle(), MakeDelegate(this, &GMenu2X::editLink)});
		options.push_back((MenuOption){tr["Delete"] + " " + menu->getLink()->getTitle(), MakeDelegate(this, &GMenu2X::deleteLink)});
	}

	options.push_back((MenuOption){tr["Add link"], 			MakeDelegate(this, &GMenu2X::addLink)});
	options.push_back((MenuOption){tr["Add section"],		MakeDelegate(this, &GMenu2X::addSection)});
	options.push_back((MenuOption){tr["Rename section"],		MakeDelegate(this, &GMenu2X::renameSection)});
	options.push_back((MenuOption){tr["Delete section"],		MakeDelegate(this, &GMenu2X::deleteSection)});

	if (!platform->opk.empty()) {
		options.push_back((MenuOption){tr["Update package links"],	MakeDelegate(this, &GMenu2X::pkgScanner)});
	}

	MessageBox mb(this, options);
}

void GMenu2X::addLink() {
	BrowseDialog bd(this, tr["Add link"], tr["Select an application"]);
	bd.showDirectories = true;
	bd.showFiles = true;
	string filter = ".dge,.gpu,.gpe,.sh,.bin,.elf,";

	if (platform->ipk) {
		filter = ".ipk," + filter;
	}

	if (!platform->opk.empty()) {
		filter = ".opk," + filter;
	}

	bd.setFilter(filter);
	while (bd.exec()) {
		string ext = bd.getExt(bd.selected);

		if (platform->ipk && ext == ".ipk" && file_exists("/usr/bin/opkg")) {
			ipkInstall(bd.getPath(bd.selected));
		} else if (!platform->opk.empty() && ext == ".opk") {
			opkInstall(bd.getPath(bd.selected));
		} else if (menu->addLink(bd.getPath(bd.selected))) {
			editLink();
			return;
		}
	}
	sync();
}

void GMenu2X::changeSelectorDir() {
	BrowseDialog bd(this, tr["Selector Path"], tr["Directory to start the selector"]);
	bd.showDirectories = true;
	bd.showFiles = false;
	bd.allowSelectDirectory = true;
	if (bd.exec() && bd.getDir() != "/") {
		confStr["tmp_selector"] = bd.getDir() + "/";
	}
}

void GMenu2X::editLink() {
	if (menu->getLinkApp() == NULL) return;

	vector<string> pathV;
	split(pathV, menu->getLinkApp()->getFile(), "/");
	string oldSection = "";
	if (pathV.size() > 1) oldSection = pathV[pathV.size()-2];
	string newSection = oldSection;

	string linkExec = menu->getLinkApp()->getExec();
	string linkTitle = menu->getLinkApp()->getTitle();
	string linkDescription = menu->getLinkApp()->getDescription();
	string linkIcon = menu->getLinkApp()->getIcon();
	string linkManual = menu->getLinkApp()->getManual();
	string linkParams = menu->getLinkApp()->getParams();
	string linkSelFilter = menu->getLinkApp()->getSelectorFilter();
	string linkSelDir = menu->getLinkApp()->getSelectorDir();
	bool useSelector = !linkSelDir.empty();
	bool linkSelBrowser = menu->getLinkApp()->getSelectorBrowser();
	string linkSelScreens = menu->getLinkApp()->getSelectorScreens();
	string linkSelAliases = menu->getLinkApp()->getAliasFile();
	int linkClock = menu->getLinkApp()->getCPU();
	string linkBackdrop = menu->getLinkApp()->getBackdrop();
	string dialogTitle = tr["Edit"] + " " + linkTitle;
	string dialogIcon = menu->getLinkApp()->getIconPath();
	string linkDir = dir_name(linkExec);
	int linkGamma = menu->getLinkApp()->getGamma();
	// string linkHomeDir = menu->getLinkApp()->getHomeDir();

	vector<string> scaleMode;
	// scaleMode.push_back("Crop");
	scaleMode.push_back("Stretch");
	scaleMode.push_back("Aspect");
	scaleMode.push_back("Original");
	if (((float)(platform->w)/platform->h) != (4.0f/3.0f)) scaleMode.push_back("4:3");
	string linkScaleMode = scaleMode[menu->getLinkApp()->getScaleMode()];

	vector<string> selStr;
	selStr.push_back("OFF");
	selStr.push_back("AUTO");
	if (!linkSelDir.empty())
		selStr.push_back(real_path(linkSelDir) + "/");

	string s = "";
	s += linkSelDir.back();

	if (linkSelDir.empty()) confStr["tmp_selector"] = selStr[0];
	else if (s != "/") confStr["tmp_selector"] = selStr[1];
	else confStr["tmp_selector"] = selStr[2];

	SettingsDialog sd(this, /*ts,*/ dialogTitle, dialogIcon);

	// sd.addSetting(new MenuSettingFile(this,		tr["Executable"],	tr["Application this link points to"], &linkExec, ".dge,.gpu,.gpe,.sh,.bin,.opk,.elf,", linkExec, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingString(this,		tr["Title"],			tr["Link title"], &linkTitle, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingString(this,		tr["Description"],	tr["Link description"], &linkDescription, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingMultiString(this,	tr["Section"],		tr["The section this link belongs to"], &newSection, menu->getSections()));
	sd.addSetting(new MenuSettingString(this,		tr["Parameters"],	tr["Command line arguments to pass to the application"], &linkParams, dialogTitle, dialogIcon));

	sd.addSetting(new MenuSettingImage(this,		tr["Icon"],			tr["Select a custom icon for the link"], &linkIcon, ".png,.bmp,.jpg,.jpeg,.gif", linkExec, dialogTitle, dialogIcon));

	if (platform->cpu_max > platform->cpu_min && platform->cpu_step > 0) {
		sd.addSetting(new MenuSettingInt(this,		tr["CPU Clock"],		tr["CPU clock frequency when launching this link"], &linkClock, platform->cpu_menu, platform->cpu_min, platform->cpu_max, platform->cpu_step));
	}
	// sd.addSetting(new MenuSettingDir(			this, tr["Home Path"],		tr["Set directory as $HOME for this link"], &linkHomeDir, CARD_ROOT, dialogTitle, dialogIcon));

	if (platform->hw_scaler) {
		sd.addSetting(new MenuSettingMultiString(this,	tr["Scale Mode"],	tr["Hardware scaling mode"], &linkScaleMode, scaleMode));
	}

	sd.addSetting(new MenuSettingMultiString(this,	tr["File Selector"],	tr["Use file browser selector"], &confStr["tmp_selector"], selStr, NULL, MakeDelegate(this, &GMenu2X::changeSelectorDir)));
	sd.addSetting(new MenuSettingBool(this,			tr["Show Folders"],	tr["Allow the selector to change directory"], &linkSelBrowser));
	sd.addSetting(new MenuSettingString(this,		tr["File Filter"],	tr["Filter by file extension (separate with commas)"], &linkSelFilter, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingDir(this,			tr["Box Art"],		tr["Directory of the box art for the selector"], &linkSelScreens, linkDir, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingFile(this,			tr["Aliases"],		tr["File containing a list of aliases for the selector"], &linkSelAliases, ".txt,.dat", linkExec, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingImage(this,		tr["Backdrop"],		tr["Select an image backdrop"], &linkBackdrop, ".png,.bmp,.jpg,.jpeg", linkExec, dialogTitle, dialogIcon));
	sd.addSetting(new MenuSettingFile(this,			tr["Manual"],		tr["Select a Manual or Readme file"], &linkManual, ".man.png,.txt,.me", linkExec, dialogTitle, dialogIcon));

	if (platform->gamma) {
		sd.addSetting(new MenuSettingInt(this,		tr["Gamma"],			tr["Gamma value to set when launching this link"], &linkGamma, 50, 0, 100 ));
	}

	if (sd.exec() && sd.edited() && sd.save) {
		menu->getLinkApp()->setExec(linkExec);
		menu->getLinkApp()->setTitle(linkTitle);
		menu->getLinkApp()->setDescription(linkDescription);
		menu->getLinkApp()->setIcon(linkIcon);
		menu->getLinkApp()->setManual(linkManual);
		menu->getLinkApp()->setParams(linkParams);
		menu->getLinkApp()->setSelectorFilter(linkSelFilter);
		menu->getLinkApp()->setSelectorElement(0);

		if (linkSelDir.empty()) linkSelDir = confStr["homePath"];
		if (confStr["tmp_selector"] == "OFF") linkSelDir = "";
		else if (confStr["tmp_selector"] == "AUTO") linkSelDir = real_path(linkSelDir);
		else linkSelDir = confStr["tmp_selector"];

		int scalemode = 0;
		if (linkScaleMode == "Aspect") scalemode = 1;
		else if (linkScaleMode == "Original") scalemode = 2;
		else if (linkScaleMode == "4:3") scalemode = 3;
		menu->getLinkApp()->setScaleMode(scalemode);

		menu->getLinkApp()->setSelectorDir(linkSelDir);
		menu->getLinkApp()->setSelectorBrowser(linkSelBrowser);
		menu->getLinkApp()->setSelectorScreens(linkSelScreens);
		menu->getLinkApp()->setAliasFile(linkSelAliases);
		menu->getLinkApp()->setBackdrop(linkBackdrop);
		menu->getLinkApp()->setCPU(linkClock);
		menu->getLinkApp()->setGamma(linkGamma);

		// if section changed move file and update link->file
		if (oldSection != newSection) {
			vector<string>::const_iterator newSectionIndex = find(menu->getSections().begin(), menu->getSections().end(), newSection);
			if (newSectionIndex == menu->getSections().end()) return;
			string newFileName = home_path("sections/") + newSection + "/" + linkTitle;
			uint32_t x = 2;
			while (file_exists(newFileName)) {
				string id = "";
				stringstream ss; ss << x; ss >> id;
				newFileName = home_path("sections/") + newSection + "/" + linkTitle + id;
				x++;
			}
			rename(menu->getLinkApp()->getFile().c_str(), newFileName.c_str());
			menu->getLinkApp()->renameFile(newFileName);

			INFO("New section: (%i) %s", newSectionIndex - menu->getSections().begin(), newSection.c_str());

			menu->linkChangeSection(menu->getLinkIndex(), menu->getSectionIndex(), newSectionIndex - menu->getSections().begin());
		}
		menu->getLinkApp()->save();
		sync();
	}
	confInt["section"] = menu->getSectionIndex();
	confInt["link"] = menu->getLinkIndex();
	confStr["tmp_selector"] = "";
}

void GMenu2X::deleteLink() {
	int package_type = 0;
	MessageBox mb(this, tr["Delete"] + menu->getLink()->getTitle() + "\n" + tr["THIS CAN'T BE UNDONE"] + "\n" + tr["Are you sure?"], menu->getLink()->getIconPath());
	string package = menu->getLinkApp()->getExec();

	if (!platform->opk.empty() && file_ext(package, true) == ".opk") {
		package_type = 1;
		mb.setButton(MODIFIER, tr["Uninstall OPK"]);
		goto dialog; // shameless use of goto
	}

	if (platform->ipk) {
		package = ipkName(menu->getLinkApp()->getFile());
		if (!package.empty()) {
			package_type = 2;
			mb.setButton(MODIFIER, tr["Uninstall IPK"]);
			goto dialog; // shameless use of goto
		}
	}

	dialog:
	mb.setButton(MANUAL, tr["Delete link"]);
	mb.setButton(CANCEL,  tr["No"]);

	switch (mb.exec()) {
		case MODIFIER:
			if (package_type == 1) {
				unlink(package.c_str());
				menu->deleteSelectedLink();
			} else if (package_type == 2) {
				TerminalDialog td(this, tr["Uninstall package"], "opkg remove " + package, "skin:icons/configure.png");
				td.exec("opkg remove " + package);
			}
			initMenu();
			break;
		case MANUAL:
			menu->deleteSelectedLink();
			break;
		default:
			return;
	}
}

void GMenu2X::addSection() {
	InputDialog id(this, /*ts,*/ tr["Insert a name for the new section"], "", tr["Add section"], "skin:icons/section.png");
	if (id.exec()) {
		// only if a section with the same name does not exist
		if (find(menu->getSections().begin(), menu->getSections().end(), id.getInput()) == menu->getSections().end()) {
			// section directory doesn't exists
			if (menu->addSection(id.getInput())) {
				menu->setSectionIndex(menu->getSections().size() - 1); //switch to the new section
				sync();
			}
		}
	}
}

void GMenu2X::renameSection() {
	InputDialog id(this, /*ts,*/ tr["Insert a new name for this section"], menu->getSection(), tr["Rename section"], menu->getSectionIcon());
	if (id.exec()) {
		menu->renameSection(menu->getSectionIndex(), id.getInput());
		sync();
	}
}

void GMenu2X::deleteSection() {
	MessageBox mb(this, tr["Delete section"] + " '" + menu->getSectionName() + "'\n" + tr["THIS CAN'T BE UNDONE"] + "\n" + tr["Are you sure?"], menu->getSectionIcon());
	mb.setButton(MANUAL, tr["Yes"]);
	mb.setButton(CANCEL,  tr["No"]);
	if (mb.exec() != MANUAL) return;
	if (rmtree(home_path("sections/") + menu->getSection())) {
		menu->deleteSelectedSection();
		sync();
	}
}

void GMenu2X::pkgScanner() {
	PKGScannerDialog od(this, tr["Update package links"], "Scanning packages", "skin:icons/configure.png");
	od.exec();
	initMenu();
}

void GMenu2X::opkInstall(string path) {
	input->update(false);
	bool debug = input->isActive(MENU);

	PKGScannerDialog od(this, tr["Package installer"], tr["Installing"] + " " + base_name(path), "skin:icons/configure.png");
	od.opkpath = path;
	od.exec(debug);
	initMenu();
}

string GMenu2X::ipkName(string cmd) {
	if (!file_exists("/usr/bin/opkg"))
		return "";
	char package[128] = {0,0,0,0,0,0,0,0};
	cmd = "opkg search \"*" + base_name(cmd) + "\" | cut -f1 -d' '";
	FILE *fp = popen(cmd.c_str(), "r");
	if (fp == NULL)
		return "";

	fgets(package, sizeof(package) - 1, fp);
	pclose(fp);

	if (package == NULL)
		return "";

	return trim((string)package);
}

void GMenu2X::ipkInstall(string path) {
	string cmd = "opkg install --force-reinstall --force-overwrite ";
	input->update(false);
	bool debug = input->isActive(MENU);
	if (debug) cmd += "--force-downgrade --force-depends --force-maintainer ";
	TerminalDialog td(this, tr["Package installer"], "opkg install " + base_name(path), "skin:icons/configure.png");
	td.exec(cmd + cmdclean(path));
	system("if [ -d \"$HOME/apps/gmenu2x/sections\" ]; then mkdir -p \"$HOME/.gmenunx/sections\"; cp -r \"$HOME/apps/gmenu2x/sections/\"* \"$HOME/.gmenunx/sections/\"; /*rm -rf \"$HOME/apps/gmenu2x/sections\"*/; fi; sync;");
	initMenu();
}

void GMenu2X::setInputSpeed() {
	input->setInterval(150);
	input->setInterval(300, SECTION_PREV);
	input->setInterval(300, SECTION_NEXT);
	input->setInterval(300, CONFIRM);
	input->setInterval(300, CANCEL);
	input->setInterval(300, MANUAL);
	input->setInterval(300, MODIFIER);
	input->setInterval(300, PAGEUP);
	input->setInterval(300, PAGEDOWN);
}

int GMenu2X::setVolume(int val, bool popup) {
	int volumeStep = 10;

	val = constrain(val, 0, 100);

	if (popup) {
		Surface *bg = new Surface(s);

		powerManager->clearTimer();
		while (true) {
			drawSlider(val, 0, 100, menu->getVolumeIcon(platform->getVolumeMode(val)), bg);

			input->update();

			if (input->isActive(SETTINGS) || input->isActive(CONFIRM) || input->isActive(CANCEL)) {
				break;
			} else if (input->isActive(LEFT) || input->isActive(DEC) || input->isActive(VOLDOWN) || input->isActive(SECTION_PREV)) {
				val = max(0, val - volumeStep);
			} else if (input->isActive(RIGHT) || input->isActive(INC) || input->isActive(VOLUP) || input->isActive(SECTION_NEXT)) {
				val = min(100, val + volumeStep);
			}

			val = constrain(val, 0, 100);
		}

		bg->blit(s, 0, 0);
		s->flip();

		powerManager->resetSuspendTimer();
		confInt["globalVolume"] = val;
	}

	platform->setVolume(val);

	return val;
}

int GMenu2X::setBacklight(int val, bool popup) {
	if (popup) {
		int backlightStep = 10;
		val = constrain(val, 5, 100);

		Surface *bg = new Surface(s);

		powerManager->clearTimer();
		while (true) {
			drawSlider(val, 0, 100, menu->getBrightnessIcon(val), bg);

			input->update();

			if (input->isActive(SETTINGS) || input->isActive(CONFIRM) || input->isActive(CANCEL)) {
				break;
			} else if (input->isActive(LEFT) || input->isActive(DEC) || input->isActive(SECTION_PREV)) {
				val = setBacklight(max(5, val - backlightStep), false);
			} else if (input->isActive(RIGHT) || input->isActive(INC) || input->isActive(SECTION_NEXT)) {
				val = setBacklight(min(100, val + backlightStep), false);
			} else if (input->isActive(BACKLIGHT)) {
				SDL_Delay(80);
				val = platform->getBacklight();
			}

			val = constrain(val, 5, 100);
		}

		bg->blit(s, 0, 0);
		s->flip();

		powerManager->resetSuspendTimer();
		confInt["backlight"] = val;
	}

	platform->setBacklight(val);

	return val;
}

int GMenu2X::drawButton(Button *btn, int x, int y) {
	if (y < 0) y = platform->h + y;
	btn->setPosition(x, y);
	return btn->paint();
}

int GMenu2X::drawButton(Surface *s, const string &btn, const string &text, int x, int y) {
	if (y < 0) y = platform->h + y;
	Surface *icon = sc["skin:imgs/buttons/" + btn + ".png"];
	if (icon != NULL) {
		icon->blit(s, x, y, HAlignLeft | VAlignMiddle);
		x += 19;
		if (!text.empty()) {
			s->write(font, text, x, y, VAlignMiddle, skinConfColor["fontAlt"], skinConfColor["fontAltOutline"]);
			x += font->getTextWidth(text);
		}
	}
	return x + 6;
}

int GMenu2X::drawButtonRight(Surface *s, const string &btn, const string &text, int x, int y) {
	if (y < 0) y = platform->h + y;
	Surface *icon = sc["skin:imgs/buttons/" + btn + ".png"];
	if (icon != NULL) {
		if (!text.empty()) {
			x -= font->getTextWidth(text);
			s->write(font, text, x, y, HAlignLeft | VAlignMiddle, skinConfColor["fontAlt"], skinConfColor["fontAltOutline"]);
		}
		x -= 19;
		icon->blit(s, x, y, HAlignLeft | VAlignMiddle);
	}
	return x - 6;
}

void GMenu2X::drawScrollBar(uint32_t pagesize, uint32_t totalsize, uint32_t pagepos, SDL_Rect scrollRect, const uint8_t align) {
	if (totalsize <= pagesize) return;
	SDL_Rect bar = {2, 2, 2, 2};

	if ((align & VAlignBottom) || (align & VAlignTop)) {
		bar.w = (scrollRect.w - 3) * pagesize / totalsize;
		bar.x = (scrollRect.w - 3) * pagepos / totalsize;
		bar.x = scrollRect.x + bar.x + 3;

		if (align & VAlignTop) {
			bar.y = scrollRect.y + 2; // top
		} else {
			bar.y = scrollRect.y + scrollRect.h - 4; // bottom
		}

		if (bar.w < 8) {
			bar.w = 8;
		}

		if (bar.x + bar.w > scrollRect.x + scrollRect.w - 3) {
			bar.x = scrollRect.x + scrollRect.w - bar.w - 3;
		}
	} else { // HAlignLeft || HAlignRight
		bar.h = (scrollRect.h - 3) * pagesize / totalsize;
		bar.y = (scrollRect.h - 3) * pagepos / totalsize;
		bar.y = scrollRect.y + bar.y + 3;

		if (align & HAlignRight) {
			bar.x = scrollRect.x + scrollRect.w - 4; // right
		}

		if (bar.h < 8) {
			bar.h = 8;
		}

		if (bar.y + bar.h > scrollRect.y + scrollRect.h - 3) {
			bar.y = scrollRect.y + scrollRect.h - bar.h - 3;
		}
	}

	s->box(bar, skinConfColor["selectionBg"]);
	s->rectangle(bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2, skinConfColor["listBg"]);
}

void GMenu2X::drawSlider(int val, int min, int max, string icon, Surface *bg) {
	SDL_Rect progress = {52, 32, platform->w - 84, 8};
	SDL_Rect box = {20, 20, platform->w - 40, 32};

	val = constrain(val, min, max);

	bg->blit(s,0,0);
	s->box(box, skinConfColor["messageBoxBg"]);
	s->rectangle(box.x + 2, box.y + 2, box.w - 4, box.h - 4, skinConfColor["messageBoxBorder"]);

	sc[icon]->blit(s, 28, 28);

	s->box(progress, skinConfColor["messageBoxBg"]);
	s->box(progress.x + 1, progress.y + 1, val * (progress.w - 3) / max + 1, progress.h - 2, skinConfColor["messageBoxSelection"]);
	s->flip();
}

uint32_t GMenu2X::timerFlip(uint32_t interval, void *param) {
	instance->s->flip();
	return 0;
};
