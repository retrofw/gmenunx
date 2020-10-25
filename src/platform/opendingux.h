#ifndef PLATFORM_OPENDINGUX_H
#define PLATFORM_OPENDINGUX_H

class OpenDingux : public Platform {
private:

public:
	OpenDingux(GMenu2X *gmenu2x): Platform(gmenu2x) {
		WARNING("OpenDingux");
	};

	int32_t tickBattery = 0;
	void hwInit() {
		rtc = true;
		tvout = false;
		udc = true;
		ext_sd = true;
		hw_scaler = true;
		gamma = false;
		cpu_menu = 0;
		cpu_link = 0;
		cpu_max = 0;
		cpu_min = 0;
		cpu_step = 0;

		w = 320;
		h = 240;
		bpp = 32;

		batteryStatus = getBatteryStatus(getBatteryLevel(), 0, 0);
		udcStatus = getUDCStatus();
		numJoyPrev = numJoy = getDevStatus();
		volumeModePrev = volumeMode = getVolumeMode(gmenu2x->confInt["globalVolume"]);
		mmcPrev = mmcStatus = getMMCStatus();

		if (file_exists("/usr/bin/retrofw")) {
			opk = "retrofw";
			ipk = true;
		} else {
			opk = "gcw0";
			ipk = false;
		}

#if defined(OPK_SUPPORT)
		system("umount -fl /mnt");
#endif
	}

	uint8_t getMMCStatus() {
		if (file_exists("/dev/mmcblk1p1")) return MMC_INSERT;
		return MMC_REMOVE;
	}

	uint8_t getUDCStatus() {
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

	uint8_t getVolumeMode(uint8_t vol) {
		if (!vol) return VOLUME_MODE_MUTE;
		return VOLUME_MODE_NORMAL;
	}

	int16_t getBatteryLevel() {
		int val = -1;
		FILE *f;
		if (getUDCStatus() == UDC_REMOVE && (f = fopen("/sys/class/power_supply/battery/capacity", "r"))) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val;
	};

	uint8_t getBatteryStatus(int32_t val, int32_t min, int32_t max) {
		if ((val > 10000) || (val < 0)) return 6;
		if (val > 90) return 5; // 100%
		if (val > 75) return 4; // 80%
		if (val > 55) return 3; // 55%
		if (val > 30) return 2; // 30%
		if (val > 15) return 1; // 15%
		return 0; // 0% :(
	}

	uint32_t hwCheck(unsigned int interval = 0, void *param = NULL) {
		tickBattery++;
		if (tickBattery > 30) { // update battery level every 30 hwChecks
			tickBattery = 0;
			batteryStatus = getBatteryStatus(getBatteryLevel(), 0, 0);
		}

		if (tickBattery > 2) {
			udcStatus = getUDCStatus();
			if (udcPrev != udcStatus) {
				udcPrev = udcStatus;
				InputManager::pushEvent(udcStatus);
				return 2000;
			}

			numJoy = getDevStatus();
			if (numJoyPrev != numJoy) {
				numJoyPrev = numJoy;
				SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
				SDL_InitSubSystem(SDL_INIT_JOYSTICK);
				InputManager::pushEvent(JOYSTICK_CONNECT);
				return 5000;
			}

			volumeMode = getVolumeMode(1);
			if (volumeModePrev != volumeMode) {
				volumeModePrev = volumeMode;
				InputManager::pushEvent(PHONES_CONNECT);
				return 500;
			}

			mmcStatus = getMMCStatus();
			if (mmcPrev != mmcStatus) {
				mmcPrev = mmcStatus;
				InputManager::pushEvent(mmcStatus);
				if (mmcStatus == MMC_REMOVE) {
					system("umount -fl /mnt &> /dev/null");
				}
				return 500;
			}
		}
		return interval;
	}

	void setUDC(int udcStatus) {
		if (udcStatus == UDC_REMOVE) {
			INFO("USB Disconnected. Disabling devices...");
			system("/usr/bin/retrofw stop");
			return;
		}

		INFO("Enabling networking device");
		system("/usr/bin/retrofw network on");
		gmenu2x->inetIcon = gmenu2x->sc["skin:imgs/inet.png"];
	}

	int16_t getBacklight() {
		int val = -1;
		if (FILE *f = fopen("/sys/class/backlight/pwm-backlight/brightness", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val * (100.0f / 255.0f);
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

		volumeMode = getVolumeMode(val);
	}

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
};

#endif
