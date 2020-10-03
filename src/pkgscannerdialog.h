#ifndef PKGSCANNERDIALOG_H_
#define PKGSCANNERDIALOG_H_

#include <string>
#include "gmenu2x.h"
#include "textdialog.h"

using std::string;
using std::vector;

class GMenu2X;

class PKGScannerDialog : public TextDialog {
public:
	string opkpath = "";
	void opkInstall(const string &path);
	void opkScan(string opkdir);
	PKGScannerDialog(GMenu2X *gmenu2x, const string &title, const string &description, const string &icon, const string &backdrop = "");
	void preProcess() { };
	void exec(bool any_platform = false);
};

#endif /*PKGSCANNERDIALOG_H_*/
