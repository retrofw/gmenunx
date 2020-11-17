#include "dialog.h"
#include "gmenu2x.h"
#include "debug.h"

Dialog::Dialog(GMenu2X *gmenu2x, const std::string &title, const std::string &description, const std::string &icon):
gmenu2x(gmenu2x), title(title), description(description), icon(icon) {
	bg = new Surface(gmenu2x->bg);

	buttons.clear();
	gmenu2x->input->dropEvents(); // prevent passing input away
}

Dialog::~Dialog() {
	gmenu2x->input->dropEvents(); // prevent passing input away
	delete bg;
}

void Dialog::drawTopBar(Surface *s, const std::string &title, const std::string &description, const std::string &icon) {
	Surface *bar = gmenu2x->sc[gmenu2x->sc.getSkinFilePath("imgs/topbar.png", false)];
	if (bar != NULL) {
		bar->blit(s, gmenu2x->platform->w / 2, 0, HAlignCenter);
	} else {
		s->box(0, 0, gmenu2x->platform->w, gmenu2x->skinConfInt["sectionBarSize"], gmenu2x->skinConfColor["topBarBg"]);
	}

	s->setClipRect({0, 0, gmenu2x->platform->w, gmenu2x->skinConfInt["sectionBarSize"]});

	int iconOffset = 2;

	if (!icon.empty() && gmenu2x->skinConfInt["showDialogIcon"]) { // drawTitleIcon
		iconOffset = gmenu2x->skinConfInt["sectionBarSize"];

		Surface *i = gmenu2x->sc.add(icon, icon + "dialog");

		if (i == NULL) i = gmenu2x->sc["skin:" + icon];
		if (i == NULL) i = gmenu2x->sc["skin:icons/generic.png"];

		if (i->width() > iconOffset - 8 || i->height() > iconOffset - 8)
			i->softStretch(iconOffset - 8, iconOffset - 8, SScaleFit);

		gmenu2x->s->setClipRect({4, 4, iconOffset - 8, iconOffset - 8});
		i->blit(s, {4, 4, iconOffset - 8, iconOffset - 8}, HAlignCenter | VAlignMiddle);
		gmenu2x->s->clearClipRect();
	}

	if (!title.empty()) // writeTitle
		s->write(gmenu2x->titlefont, title, iconOffset, 2, VAlignTop, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);

	if (!description.empty()) // writeSubTitle
		s->write(gmenu2x->font, description, iconOffset, gmenu2x->skinConfInt["sectionBarSize"] - 2, VAlignBottom, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);

	s->clearClipRect();
}

void Dialog::drawBottomBar(Surface *s, buttons_t buttons) {
	Surface *bar = gmenu2x->sc[gmenu2x->sc.getSkinFilePath("imgs/bottombar.png", false)];
	if (bar != NULL) {
		bar->blit(s, gmenu2x->platform->w / 2, gmenu2x->platform->h, HAlignCenter | VAlignBottom);
	} else {
		s->box(gmenu2x->bottomBarRect, gmenu2x->skinConfColor["bottomBarBg"]);
	}

	int x = gmenu2x->bottomBarRect.x + 5, y = gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2;

	for (const auto &itr: buttons) {
		Surface *btn;
		string path = itr[0];
		if (path.substr(0, 5) != "skin:") {
			path = "skin:imgs/buttons/" + path + ".png";
		}
		btn = gmenu2x->sc[path];

		string txt = itr[1];
		if (btn != NULL) {
			btn->blit(s, x, y, HAlignLeft | VAlignMiddle);
			x += btn->width() + 4;
		}
		if (!txt.empty()) {
			s->write(gmenu2x->font, txt, x, y, VAlignMiddle, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);
			x += gmenu2x->font->getTextWidth(txt);
		}
		x += 6;
	}
}

void Dialog::drawDialog(Surface *s, bool top, bool bottom) {
	if (s == NULL) s = gmenu2x->s;

	this->bg->blit(s, 0, 0);

	if (top) drawTopBar(s, title, description, icon);
	if (bottom) drawBottomBar(s, buttons);
	s->box(gmenu2x->linksRect, gmenu2x->skinConfColor["listBg"]);
}
