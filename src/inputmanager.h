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
#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

enum actions {
	WAKE_UP,
	UP, DOWN, LEFT, RIGHT,
	CONFIRM, CANCEL, MANUAL, MODIFIER,
	SECTION_PREV, SECTION_NEXT,
	INC, DEC,
	PAGEUP, PAGEDOWN,
	SETTINGS, MENU,
	VOLUP, VOLDOWN,
	BACKLIGHT, POWER,
	UDC_CONNECT, // = NUM_ACTIONS,
	UDC_REMOVE,
	MMC_INSERT,
	MMC_REMOVE,
	TV_CONNECT,
	TV_REMOVE,
	PHONES_CONNECT,
	PHONES_REMOVE,
	JOYSTICK_CONNECT,
	JOYSTICK_REMOVE,
	SCREENSHOT,
	NUM_ACTIONS
};

#define VOLUME_HOTKEY		SECTION_PREV
#define BACKLIGHT_HOTKEY	SECTION_NEXT

#include <SDL.h>
#include <vector>
#include <string>

class GMenu2X;

using std::vector;
using std::string;

typedef struct {
	int type;
	uint32_t num;
	int value;
	int treshold;
} InputMap;

typedef vector<InputMap> MappingList;

typedef struct {
	bool active;
	int interval;
	MappingList maplist;
} InputManagerAction;

class InputManager {
private:
	InputMap getInputMapping(int action);

	vector <SDL_Joystick*> joysticks;
	vector <InputManagerAction> actions;

	const char konami[10] = {UP, UP, DOWN, DOWN, LEFT, RIGHT, LEFT, RIGHT, CANCEL, CONFIRM}; // eegg
	char input_combo[10] = {POWER}; // eegg
	GMenu2X *gmenu2x;

public:
	uint16_t battery = 3;
	uint16_t mmc_, mmc;
	uint16_t udc_ = UDC_REMOVE, udc;
	uint8_t devices_, devices; // number of connected joysticks
	uint16_t volume_mode_, volume_mode;
	uint16_t tvout_ = TV_REMOVE, tvout;
	int32_t ticks = 0;

	static uint32_t wakeUp(uint32_t interval, void *repeat);

	InputManager(GMenu2X *gmenu2x, string conffile);
	~InputManager();
	void initJoysticks(bool reinit = false);

	bool update(bool wait = true);
	bool combo();
	void dropEvents(bool drop_timer = true);
	static void pushEvent(size_t action);
	void setActionsCount(int count);
	void setInterval(int ms, int action = -1);
	bool &operator[](int action);
	bool scanAction(int action);
	bool isActive(int action);
	void setActive(int action);
	void initHardwareInput();
	uint32_t hardwareMonitor();
};

#endif
