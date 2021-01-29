#ifndef PLATFORM_OPENDINGUX_H
#define PLATFORM_OPENDINGUX_H

class OpenDingux : public Platform {
private:

public:
	OpenDingux(GMenu2X *gmenu2x): Platform(gmenu2x) {
		WARNING("OpenDingux");

		rtc = true;
		tvout = false;
		udc = false;
		ext_sd = true;
		hw_scaler = true;

		w = 320;
		h = 240;
		bpp = 32;

		if (file_exists("/usr/bin/retrofw")) {
			system("[ -d /home/retrofw ] && mount -o remount,async /home/retrofw");
			opk = "retrofw";
			ipk = true;
		} else {
			opk = "gcw0";
			ipk = false;
		}

		if (FILE *f = fopen("/sys/devices/platform/jz-lcd.0/allow_downscaling", "w")) {
			fprintf(f, "1");
			fclose(f);
		}

#if defined(OPK_SUPPORT)
		system("umount -fl /mnt");
#endif
	}

	uint8_t getMMC() {
		if (FILE *f = fopen("/dev/mmcblk1p1", "r")) {
			fclose(f);
			return MMC_INSERT;
		}
		return MMC_REMOVE;
	}

	uint8_t getUDC() {
		int val = -1;
		if (FILE *f = fopen("/sys/class/power_supply/usb/online", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
			if (val == 1) {
				return UDC_CONNECT;
			}
		}
		return UDC_REMOVE;
	}

	void setUDC(int udc) {
		if (udc == UDC_REMOVE) {
			INFO("USB Disconnected. Disabling devices...");
			system("/usr/bin/retrofw stop");
			return;
		}

		INFO("Enabling networking device");
		system("/usr/bin/retrofw network on");
		gmenu2x->inetIcon = "skin:imgs/inet.png";
	}

	void enableTerminal() {
		/* Enable the framebuffer console */
		char c = '1';
		int fd = open("/sys/devices/virtual/vtconsole/vtcon1/bind", O_WRONLY);
		if (fd) {
			write(fd, &c, 1);
			close(fd);
		}

		fd = open("/dev/tty1", O_RDWR);
		if (fd) {
			ioctl(fd, VT_ACTIVATE, 1);
			close(fd);
		}
	}

	int16_t getBattery(bool raw) {
		int val = -1;
		FILE *f;
		if (getUDC() == UDC_REMOVE && (f = fopen("/sys/class/power_supply/battery/capacity", "r"))) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		if (raw) return val;
		if ((val > 10000) || (val < 0)) return 6;
		if (val > 90) return 5; // 100%
		if (val > 75) return 4; // 80%
		if (val > 55) return 3; // 55%
		if (val > 30) return 2; // 30%
		if (val > 15) return 1; // 15%
		return 0; // 0% :(
	}

	void setScaleMode(unsigned int mode) {
		if (FILE *f = fopen("/sys/devices/platform/jz-lcd.0/keep_aspect_ratio", "w")) {
			fprintf(f, "%d", (mode == 1));
			fclose(f);
		}

		if (FILE *f = fopen("/sys/devices/platform/jz-lcd.0/integer_scaling", "w")) {
			fprintf(f, "%d", (mode == 2));
			fclose(f);
		}
	}

	void setBacklight(int val) {
		if (FILE *f = fopen("/sys/class/backlight/pwm-backlight/brightness", "w")) {
			fprintf(f, "%0.0f", val * (255.0f / 100.0f)); // fputs(val, f);
			fclose(f);
		}

		if (FILE *f = fopen("/sys/class/graphics/fb0/blank", "w")) {
			fprintf(f, "%d", val <= 0);
			fclose(f);
		}
	}

	int16_t getBacklight() {
		int val = -1;
		if (FILE *f = fopen("/sys/class/backlight/pwm-backlight/brightness", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val * (100.0f / 255.0f);
	}

	void setVolume(int val) {
		val = val * (63.0f / 100.0f);

		int hp = 0;
		char cmd[96];

		if (val > 32) {
			hp = 31;
			val -= 32;
		} else if (val > 1) {
			hp = val;
			val = 1;
		}

		sprintf(cmd, "amixer set Headphone %d; amixer set PCM %d", hp, val);
		system(cmd);
	}

#if 0
	int getVolume() {
		int pcm = 0, hp = 0;
		// if (FILE *f = popen("alsa-getvolume default PCM", "r")) {
		if (FILE *f = popen("amixer get PCM | grep -i \"Playback [0-9] \\[\" | cut -f 5 -d \" \" | head -n 1", "r")) {
			fscanf(f, "%i", &pcm);
			pclose(f);
		}
		// if (FILE *f = popen("alsa-getvolume default Headphone", "r")) {
		if (FILE *f = popen("amixer get Headphone | grep -i \"Playback [0-9] \\[\" | cut -f 5 -d \" \" | head -n 1", "r")) {
			fscanf(f, "%i", &hp);
			pclose(f);
		}

		return (pcm + hp) * (100.0f / 63.0f);
	}
#endif

	string hwPreLinkLaunch() {
		system("[ -d /home/retrofw ] && mount -o remount,sync /home/retrofw");
		return "";
	}
};

#endif
