PLATFORM	?= linux
BUILDTIME	:= $(shell date +%s)

CC			:= $(CROSS_COMPILE)gcc
CXX			:= $(CROSS_COMPILE)g++
STRIP		:= $(CROSS_COMPILE)strip

SYSROOT     ?= $(shell $(CC) --print-sysroot)
SDL_CFLAGS  ?= $(shell $(SYSROOT)/usr/bin/sdl-config --cflags)
SDL_LIBS    ?= $(shell $(SYSROOT)/usr/bin/sdl-config --libs)

CFLAGS += $(SDL_CFLAGS)
CFLAGS += -DPLATFORM=\"$(PLATFORM)\" -D__BUILDTIME__="$(BUILDTIME)" -DLOG_LEVEL=4
CFLAGS += -Wundef -Wno-deprecated -Wno-unknown-pragmas -Wno-format -Wno-narrowing
CFLAGS += -std=c++11
CFLAGS += -fdata-sections -ffunction-sections -fno-exceptions -fno-math-errno -fno-threadsafe-statics
CFLAGS += -Isrc -Isrc/libopk -Isrc/platform
CFLAGS += -D_GLIBCXX_USE_CXX11_ABI=0

LDFLAGS += -Wl,-Bstatic -Lsrc/libopk -l:libopk.a
LDFLAGS += -Wl,-Bdynamic -lz $(SDL_LIBS) -lSDL_image -lSDL_ttf
LDFLAGS += -Wl,--as-needed -Wl,--gc-sections

OBJDIR = /tmp/gmenunx/$(PLATFORM)
DISTDIR = dist/$(PLATFORM)
TARGET = dist/$(PLATFORM)/gmenunx

SOURCES := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, $(OBJDIR)/src/%.o, $(SOURCES))

# File types rules
$(OBJDIR)/src/%.o: src/%.cpp src/%.h
	$(CXX) $(CFLAGS) -o $@ -c $<

# -include $(patsubst src/%.cpp, $(OBJDIR)/src/%.d, $(SOURCES))

all: dir shared

dir:
	@mkdir -p $(OBJDIR)/src dist/$(PLATFORM)

debug: $(OBJS)
	@echo "Linking gmenunx-debug..."
	$(CXX) -o $(TARGET)-debug $(OBJS) $(LDFLAGS)

shared: debug
	$(STRIP) $(TARGET)-debug -o $(TARGET)

clean:
	rm -rf $(OBJDIR) $(DISTDIR) *.gcda *.gcno $(TARGET) $(TARGET)-debug
	rm -rf /tmp/.gmenu-ipk/ dist/gmenunx-$(PLATFORM).zip dist/gmenunx-$(PLATFORM).ipk

dist: dir shared
	xgettext  --keyword=_ --from-code=UTF-8 -o /tmp/gmenunx.pot src/*.cpp
	install -m644 -D about.txt $(DISTDIR)/about.txt
	install -m644 -D COPYING $(DISTDIR)/COPYING
	mkdir -p $(DISTDIR)/skins/Default
	cp -RH assets/skins/RetroFW/* $(DISTDIR)/skins/Default
	cp -RH assets/skins/Default/font.ttf $(DISTDIR)/skins/Default
	cp -RH assets/translations $(DISTDIR)
	cp -RH assets/$(PLATFORM)/input.conf $(DISTDIR)

zip: dist
	cd $(DISTDIR)/ && rm -f ../gmenunx-$(PLATFORM).zip && zip -r ../gmenunx-$(PLATFORM).zip skins translations COPYING gmenunx input.conf gmenunx.conf about.txt

ipk: dist
	rm -rf /tmp/.gmenu-ipk/; mkdir -p /tmp/.gmenu-ipk/
	sed "s/^Version:.*/Version: $$(date +%Y%m%d)/" assets/control > /tmp/.gmenu-ipk/control
	cp assets/conffiles /tmp/.gmenu-ipk/
	echo -e "#!/bin/sh\nsync; echo -e 'Installing gmenunx..'; mount -o remount,rw /; rm /var/lib/opkg/info/gmenunx.list; exit 0" > /tmp/.gmenu-ipk/preinst
	echo -e "#!/bin/sh\nsync; mount -o remount,ro /; echo -e 'Installation finished.\nRestarting gmenunx..'; sleep 1; killall gmenunx; exit 0" > /tmp/.gmenu-ipk/postinst
	chmod +x /tmp/.gmenu-ipk/postinst /tmp/.gmenu-ipk/preinst
	tar --owner=0 --group=0 -czvf /tmp/.gmenu-ipk/control.tar.gz -C /tmp/.gmenu-ipk/ control conffiles postinst preinst
	tar --owner=0 --group=0 -czvf /tmp/.gmenu-ipk/data.tar.gz -C $(DISTDIR) about.txt COPYING gmenunx gmenunx.conf input.conf skins translations
	echo 2.0 > /tmp/.gmenu-ipk/debian-binary
	ar r dist/gmenunx-$(PLATFORM).ipk /tmp/.gmenu-ipk/control.tar.gz /tmp/.gmenu-ipk/data.tar.gz /tmp/.gmenu-ipk/debian-binary
