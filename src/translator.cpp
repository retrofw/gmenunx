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

#include <fstream>
#include <stdarg.h>

#include "utilities.h"
#include "translator.h"
#include "filelister.h"
#include "debug.h"

using std::ifstream;
using std::to_string;

void Translator::setLang(const string &lang) {
	if (lang.empty()) return;

	translations.clear();

	string line = data_path("translations/" + lang);
	ifstream f(line, std::ios_base::in);
	if (f.is_open()) {
		while (getline(f, line, '\n')) {
			line = trim(line);
			if (line == "") continue;
			if (line[0] == '#') continue;

			string::size_type position = line.find("=");
			translations[trim(line.substr(0, position))] = trim(line.substr(position + 1));
		}
		f.close();
	}
	_lang = lang;
}

string Translator::translate(const string &term, const char *replacestr, ...) {
	string result = term;

	if (_lang != "English") {
		unordered_map<string, string>::iterator i = translations.find(term);
		if (i != translations.end()) {
			result = i->second;
		}
		//else WARNING("Untranslated string: '%s'", term.c_str());
	}

	va_list arglist;
	va_start(arglist, replacestr);
	const char *param = replacestr;
	int argnum = 1;
	while (param != NULL) {
		result = strreplace(result, "$" + std::to_string(argnum), param);
		param = va_arg(arglist, const char*);
		argnum++;
	}
	va_end(arglist);

	return result;
}

string Translator::operator[](const string &term) {
	return translate(term);
}

string Translator::getLang() {
	return _lang;
}

vector<string> Translator::getLanguages() {
	FileLister fl;
	fl.showDirectories = false;
	fl.setFilter(",");
	fl.addExclude("English");
	fl.browse(data_path("translations"));
	fl.insertFile("English");
	return fl.getFiles();
}
