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
#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include "utilities.h"

#include <libintl.h>
#include <locale.h>


#define _(str) gettext(str)
// #define _F(fmt, ...) ({sprintf(tr_buf, gettext(fmt), __VA_ARGS__); tr_buf;})


const char* _F(const char *buf, ...);












// char* _f(const char *format, ...) {
//   va_list args;
//   char buffer[512];

//   va_start(args, format);
//   vsnprintf(buffer, sizeof buffer, format, args);
//   va_end(args);
//   FlushFunnyStream(buffer);
// }

// GCC has a feature called statement expressions

// So if define macro like

// #define FOO(A) ({int retval; retval = do_something(A); retval;})

// then you will be able to use it like

// foo = FOO(bar);







/**
Hash Map of translation strings.

	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
// class Translator {
// private:
// 	string _lang;
// 	unordered_map<string, string> translations;

// public:
// 	Translator(const string &lang="");
// 	~Translator();

// 	string lang();
// 	void setLang(const string &lang);
// 	bool exists(const string &term);
// 	string translate(const string &term,const char *replacestr=NULL,...);
// 	string operator[](const string &term);
// };

#endif
