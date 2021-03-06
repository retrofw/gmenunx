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
#ifndef SURFACECOLLECTION_H
#define SURFACECOLLECTION_H

#include <string>
#include <tr1/unordered_map>
using std::string;

class Surface;

typedef std::tr1::unordered_map<string, Surface *> SurfaceHash;

/**
Hash Map of surfaces that loads surfaces not already loaded and reuses already loaded ones.

	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
class SurfaceCollection {
private:
	SurfaceHash surfaces;
	string skin;

public:
	SurfaceCollection();
	void setSkin(const string &skin);
	string getSkinFilePath(const string &file, bool falback = true);
	void debug();
	Surface *add(Surface *s, const string &path);
	Surface *add(string path, string key="");
	bool del(const string &key);
	void clear();
	void move(const string &from, const string &to);
	bool exists(const string &path);
	Surface *operator[](const string &);
};

#endif
