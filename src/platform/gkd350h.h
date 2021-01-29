#ifndef PLATFORM_GKD350H_H
#define PLATFORM_GKD350H_H

#include <linux/soundcard.h>

class GKD350H : public Platform {
private:
	static const uint32_t MBASE = 0x10000000;
	static const uint32_t MSIZE = 0x1000;
	static const uint32_t CPPCR = (0x10 >> 2);

public:
	GKD350H(GMenu2X *gmenu2x): Platform(gmenu2x) {
		WARNING("GKD350H");

		rtc = true;
		ext_sd = true;
		cpu_menu = 0;
		cpu_link = 0;
		cpu_max = 7;
		cpu_min = 0;
		cpu_step = 1;

		w = 320;
		h = 240;
		bpp = 16;

		if (file_exists("/usr/bin/retrofw")) {
			system("[ -d /home/retrofw ] && mount -o remount,async /home/retrofw");
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

	uint8_t getMMC() {
		if (FILE *f = fopen("/dev/mmcblk1p1", "r")) {
			fclose(f);
			return MMC_INSERT;
		}
		return MMC_REMOVE;
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

	void setVolume(int val) {
		uint32_t soundDev = open("/dev/mixer", O_RDWR);
		if (soundDev) {
			int vol = (val << 8) | val;
			ioctl(soundDev, SOUND_MIXER_WRITE_VOLUME, &vol);
			close(soundDev);

		}
	}

	void setBacklight(int val) {
		if (FILE *f = fopen("/sys/devices/platform/jz-pwm-dev.0/jz-pwm/pwm0/dutyratio", "a")) {
			fprintf(f, "%d", val);
			fclose(f);
		}

		if (FILE *f = fopen("/sys/class/graphics/fb0/blank", "a")) {
			fprintf(f, "%d", val <= 0);
			fclose(f);
		}
	}

	void setCPU(uint32_t m) {
		// https://raw.githubusercontent.com/jz47xx/jz47xx.github.io/master/manuals/X1830_pm.pdf#384
		volatile uint8_t memdev = open("/dev/mem", O_RDWR);
		if (memdev <= 0) {
			WARNING("Could not open /dev/mem");
			return;
		}

		uint32_t *mem = (uint32_t*)mmap(0, MSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memdev, MBASE);
		if (mem != MAP_FAILED) {
			m = constrain(m, cpu_min, cpu_max);

			DEBUG("Setting clock mode %d", m);
			m = 95 + (3 * m);
			mem[CPPCR] = (m << 20) | (mem[CPPCR] & 0xFFFFF);

			munmap(mem, MSIZE);
		}
		close(memdev);
	}

	string hwPreLinkLaunch() {
		system("[ -d /home/retrofw ] && mount -o remount,sync /home/retrofw");
		return "";
	}
};

#endif
