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
#include "menusettingbool.h"
#include "gmenu2x.h"
using fastdelegate::MakeDelegate;

MenuSettingBool::MenuSettingBool(GMenu2X *gmenu2x, const string &name, const string &description, int *value)
	: MenuSetting(gmenu2x, name, description)
{
	_ivalue = value;
	_value = NULL;
	originalValue = *value != 0;
	setValue(this->value());
	initButton();
}

MenuSettingBool::MenuSettingBool(GMenu2X *gmenu2x, const string &name, const string &description, bool *value)
	: MenuSetting(gmenu2x, name, description)
{
	_value = value;
	_ivalue = NULL;
	originalValue = *value;
	setValue(this->value());
	initButton();
}

void MenuSettingBool::initButton()
{
	IconButton *btn;
	ButtonAction actionToggle = MakeDelegate(this, &MenuSettingBool::toggle);

	btn = new IconButton(gmenu2x, "skin:imgs/buttons/left.png");
	btn->setAction(actionToggle);
	buttonBox.add(btn);

	btn = new IconButton(gmenu2x, "skin:imgs/buttons/right.png", gmenu2x->tr["Change value"]);
	btn->setAction(actionToggle);
	buttonBox.add(btn);
}

void MenuSettingBool::draw(int y)
{
	MenuSetting::draw(y);
	gmenu2x->s->write( gmenu2x->font, strvalue, 155, y+gmenu2x->font->getHalfHeight(), VAlignMiddle );
}

uint MenuSettingBool::manageInput()
{
	if ( gmenu2x->input[LEFT] || gmenu2x->input[RIGHT] )
		toggle();
}

void MenuSettingBool::toggle()
{
	setValue(!value());
}

void MenuSettingBool::setValue(int value)
{
	setValue(value != 0);
}

void MenuSettingBool::setValue(bool value)
{
	if (_value == NULL)
		*_ivalue = value;
	else
		*_value = value;
	strvalue = value ? "ON" : "OFF";
}

bool MenuSettingBool::value()
{
	if (_value == NULL)
		return *_ivalue != 0;
	return *_value;
}

bool MenuSettingBool::edited()
{
	return originalValue != value();
}
