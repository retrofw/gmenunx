#ifndef PLATFORM_RETROFW_H
#define PLATFORM_RETROFW_H

#include "messagebox.h"

#include <linux/soundcard.h>


/*	RetroGame Key Codes. pingflood, 2018
	BUTTON     GMENU          SDL             NUMERIC   GPIO
	-----------------------------------------------------------------------------
	X          MODIFIER       SDLK_SPACE      32        !(mem[PEPIN] >> 07 & 1)
	A          CONFIRM        SDLK_LCTRL      306       !(mem[PDPIN] >> 22 & 1)
	B          CANCEL         SDLK_LALT       308       !(mem[PDPIN] >> 23 & 1)
	Y          MANUAL         SDLK_LSHIFT     304       !(mem[PEPIN] >> 11 & 1)
	L          SECTION_PREV   SDLK_TAB        9         !(mem[PBPIN] >> 23 & 1)
	R          SECTION_NEXT   SDLK_BACKSPACE  8         !(mem[PDPIN] >> 24 & 1)
	START      SETTINGS       SDLK_RETURN     13         (mem[PDPIN] >> 18 & 1)
	SELECT     MENU           SDLK_ESCAPE     27         (mem[PDPIN] >> 17 & 1)
	BACKLIGHT  BACKLIGHT      SDLK_3          51        !(mem[PDPIN] >> 21 & 1)
	POWER      POWER          SDLK_END        279       !(mem[PAPIN] >> 30 & 1)
	UP         UP             SDLK_UP         273       !(mem[PBPIN] >> 25 & 1)
	DOWN       DOWN           SDLK_DOWN       274       !(mem[PBPIN] >> 24 & 1)
	RIGHT      RIGHT          SDLK_RIGHT      275       !(mem[PBPIN] >> 26 & 1)
	LEFT       LEFT           SDLK_LEFT       276       !(mem[PDPIN] >> 00 & 1)
*/

void printbin(const char *id, int n) {
	printf("%s: 0x%08x ", id, n);
	for(int i = 31; i >= 0; i--) {
		printf("%d", !!(n & 1 << i));
		if (!(i % 8)) printf(" ");
	}
	printf("\e[0K\n");
}

// class BrowseDialog : protected Dialog, public FileLister {
class RetroFW : public Platform {
private:
	static const uint32_t MBASE = 0x10000000;
	static const uint32_t MSIZE = 0x20000;
	static const uint32_t CPPCR = (0x10 >> 2);
	static const uint32_t PAPIN = (0x10000 >> 2);
	static const uint32_t PBPIN = (0x10100 >> 2);
	static const uint32_t PCPIN = (0x10200 >> 2);
	static const uint32_t PDPIN = (0x10300 >> 2);
	static const uint32_t PEPIN = (0x10400 >> 2);
	static const uint32_t PFPIN = (0x10500 >> 2);
	uint32_t *mem;
	volatile uint8_t memdev = 0;

public:
	RetroFW(GMenu2X *gmenu2x) : Platform(gmenu2x) {
		WARNING("RetroFW");

		rtc = true;
		tvout = true;
		udc = true;
		ext_sd = true;
		hw_scaler = true;
		opk = "retrofw";
		ipk = true;
		cpu_menu = 528;
		cpu_link = 600;
		cpu_max = cpu_menu * 2;
		cpu_min = cpu_menu / 2;
		cpu_step = 6;

		w = 320;
		h = 240;
		bpp = 16;

		system("[ -d /home/retrofw ] && mount -o remount,async /home/retrofw");
#if defined(OPK_SUPPORT)
		system("umount -fl /mnt &> /dev/null");
#endif

		memdev = open("/dev/mem", O_RDWR);
		if (memdev > 0) {
			mem = (uint32_t*)mmap(0, MSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memdev, MBASE);
			if (mem == MAP_FAILED) {
				ERROR("Could not mmap hardware registers!");
				close(memdev);
			}
		} else {
			WARNING("Could not open /dev/mem");
		}

		struct fb_var_screeninfo vinfo;
		int fbdev = open("/dev/fb0", O_RDWR);
		if (fbdev >= 0 && ioctl(fbdev, FBIOGET_VSCREENINFO, &vinfo) >= 0) {
			w = vinfo.width;
			h = vinfo.height;
			close(fbdev);
		}

		if (w == 320 && h == 480) h = 240;

		if (FILE *f = fopen("/proc/jz/gpio", "r")) {
			char buf[7];
			fread(buf, sizeof(char), 7, f);
			fclose(f);
			if (!strncmp(buf, "480x272", 7)) {
				fwtype = FW_RETROARCADE;
				udc = false;
			} else {
				joystick = false;
			}
		}
	};

	~RetroFW() {
		if (memdev > 0) {
			close(memdev);
			if (mem != MAP_FAILED) {
				munmap(mem, MSIZE);
			}
		}
	}

	void setUDC(int udcStatus) {
		if (udcStatus == UDC_REMOVE) {
			INFO("USB Disconnected. Disabling devices...");
			system("/usr/bin/retrofw stop");
			return;
		}

		int option;
		if (gmenu2x->confStr["usbMode"] == "Storage") option = CONFIRM;
		else if (gmenu2x->confStr["usbMode"] == "Charger") option = CANCEL;
		else {
			MessageBox mb(gmenu2x, _("USB mode"), "skin:icons/usb.png");
			mb.setButton(CANCEL,  _("Charger"));
			mb.setButton(CONFIRM, _("Storage"));
			option = mb.exec();
		}

		if (option == CONFIRM) { // storage
			INFO("Enabling storage device");
			gmenu2x->quit();
			execlp("/bin/sh", "/bin/sh", "-c", "exec /usr/bin/retrofw storage on", NULL);
			return;
		}

		INFO("Enabling networking device");
		system("/usr/bin/retrofw network on");
		gmenu2x->inetIcon = "skin:imgs/inet.png";
	}

	void setTVOut(int16_t mode) {
		if (!file_exists("/proc/jz/tvout")) return;

		if (mode < 0) {
			int option;

			mode = TV_OFF;

			if (gmenu2x->confStr["tvMode"] == "NTSC") option = CONFIRM;
			else if (gmenu2x->confStr["tvMode"] == "PAL") option = MANUAL;
			else if (gmenu2x->confStr["tvMode"] == "OFF") option = CANCEL;
			else {
				MessageBox mb(gmenu2x, _("TV-out connected. Enable?"), "skin:icons/tv.png");
				mb.setButton(CONFIRM, _("NTSC"));
				mb.setButton(MANUAL,  _("PAL"));
				mb.setButton(CANCEL,  _("OFF"));
				option = mb.exec();
			}

			switch (option) {
				case CONFIRM:
					mode = TV_NTSC;
					break;
				case MANUAL:
					mode = TV_PAL;
					break;
			}
		}

		if (mode != getTVOut()) {
			if (FILE *f = fopen("/proc/jz/tvout", "w")) {
				fprintf(f, "%d", mode); // fputs(val, f);
				fclose(f);
			}

			setBacklight(gmenu2x->confInt["backlight"]);
			exit(0);
		}
	}

	uint16_t getTVOut() {
		int val = 0;
		if (FILE *f = fopen("/proc/jz/tvout", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val;
	}

	uint8_t getTVOutStatus() {
		if (memdev > 0) {
			if (fwtype == FW_RETROARCADE && !(mem[PDPIN] >> 6 & 1)) return TV_CONNECT;
			if (!(mem[PDPIN] >> 25 & 1)) return TV_CONNECT;
		}
		return TV_REMOVE;
	}

	uint8_t getMMCStatus() {
		if (memdev > 0 && !(mem[PFPIN] >> 0 & 1)) return MMC_INSERT;
		return MMC_REMOVE;
	}

	uint8_t getUDCStatus() {
		// if (memdev > 0 && ((mem[PDPIN] >> 7 & 1) || (mem[PEPIN] >> 13 & 1))) return UDC_CONNECT;
		if (memdev > 0 && (mem[PDPIN] >> 7 & 1)) return UDC_CONNECT;
		return UDC_REMOVE;
	}

	int16_t getBatteryLevel() {
		int val = -1;
		if (FILE *f = fopen("/proc/jz/battery", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val;
	}

	uint8_t getBatteryStatus(int32_t val, int32_t min, int32_t max) {
		if ((val > 10000) || (val < 0)) return 6;
		if (val > 4000) return 5; // 100%
		if (val > 3900) return 4; // 80%
		if (val > 3800) return 3; // 60%
		if (val > 3700) return 2; // 40%
		if (val > 3520) return 1; // 20%
		return 0; // 0% :(
	}

	uint8_t getVolumeMode(uint8_t vol) {
		if (!vol) return VOLUME_MODE_MUTE;
		if (memdev > 0 && !(mem[PDPIN] >> 6 & 1)) return VOLUME_MODE_PHONES;
		return VOLUME_MODE_NORMAL;
	}

#if 0
	int getVolume() {
		int vol = -1;
		uint32_t soundDev = open("/dev/mixer", O_RDONLY);

		if (soundDev) {
			ioctl(soundDev, SOUND_MIXER_READ_VOLUME, &vol);
			close(soundDev);
			if (vol != -1) {
				// just return one channel , not both channels, they're hopefully the same anyways
				return vol & 0xFF;
			}
		}
		return vol;
	}
#endif
	
	void setVolume(int val) {
		uint32_t soundDev = open("/dev/mixer", O_RDWR);
		if (soundDev) {
			int vol = (val << 8) | val;
			ioctl(soundDev, SOUND_MIXER_WRITE_VOLUME, &vol);
			close(soundDev);

		}
		// volumeMode = getVolumeMode(val);
	}

#if 0
	int16_t getBacklight() {
		int val = -1;
		if (FILE *f = fopen("/proc/jz/backlight", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val;
	}
#endif

	void setBacklight(int val) {
		if (val < 1 && getUDCStatus() != UDC_REMOVE /* && !getTVOut() */) {
			val = 0; // suspend only if not charging and TV out is not enabled
		}

		if (FILE *f = fopen("/proc/jz/backlight", "w")) {
			if (val == 0) {
				fprintf(f, "-"); // disable backlight button
			}
			fprintf(f, "%d", val); // fputs(val, f);
			fclose(f);
		}
	}

	void setScaleMode(unsigned int mode) {
		/* Scaling Modes:
			0: stretch
			1: aspect
			2: original (fallback to aspect when downscale is needed)
			3: 4:3
		*/
		if (FILE *f = fopen("/proc/jz/ipu", "w")) {
			fprintf(f, "%d", mode); // fputs(val, f);
			fclose(f);
		}
	}

	void setCPU(uint32_t mhz) {
		if (getTVOut()) {
			WARNING("Can't overclock when TV out is enabled.");
			return;
		}
		mhz = constrain(mhz, cpu_min, cpu_max);
		if (memdev > 0) {
			DEBUG("Setting clock to %d", mhz);
			uint32_t m = mhz / 6;
			mem[CPPCR] = (m << 24) | 0x090520;
		}
	}

	string hwPreLinkLaunch() {
		system("[ -d /home/retrofw ] && mount -o remount,sync /home/retrofw");
		return "";
	}
};

#endif
