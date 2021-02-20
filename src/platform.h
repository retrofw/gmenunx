#ifndef HW_GENERIC_H
#define HW_GENERIC_H

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>

class GMenu2X;

#include "inputmanager.h"

#include <string>
#include <vector>

using std::string;

enum vol_mode_t {
	VOLUME_MODE_MUTE, VOLUME_MODE_PHONES, VOLUME_MODE_NORMAL
};

enum fwtype {
	FW_GENERIC,
	FW_RETROARCADE,
	FW_OPEN2X,
	FW_GPH,
};

class Platform {
public:
	GMenu2X *gmenu2x;
	Platform(GMenu2X *gmenu2x);
	~Platform() { };

	int fwtype = FW_GENERIC;
	bool rtc = false;
	bool tvout = false;
	bool udc = false;
	bool ext_sd = false;
	bool hw_scaler = false;
	bool ipk = false;
	bool gamma = false;
	bool joystick = true;
	bool battery = true;
	bool volume = true;
	bool backlight = true;
	int cpu_menu = 0;
	int cpu_link = 0;
	int cpu_max = 0;
	int cpu_min = 0;
	int cpu_step = 0;
	string opk = "linux";
	string data_path;
	string home_path;
	uint32_t w = 480, h = 272, bpp = 16;

	virtual uint16_t getDevices();
	virtual uint8_t getMMC() { return MMC_REMOVE; };
	virtual uint8_t getUDC() { return UDC_REMOVE; };
	virtual uint8_t getTVOut() { return TV_REMOVE; };
	virtual int16_t getBattery(bool raw = false) { return 6; };
	virtual uint8_t getVolumeMode(uint8_t vol) { if (!vol) return VOLUME_MODE_MUTE; return VOLUME_MODE_NORMAL; };
	virtual void setVolume(int val) { };
	virtual void setBacklight(int val) { };
	virtual int16_t getBacklight() { return -1; };
	virtual void setScaleMode(unsigned int mode) { };
	virtual void setCPU(uint32_t mhz) { };
	virtual void enableTerminal() { };
	virtual void setGamma(int value) { };
	virtual void setUDC(int mode = -1) { };
	virtual void setTVOut(int mode = -1) { };
	virtual string hwPreLinkLaunch() { return ""; };
};

Platform* PlatformInit(GMenu2X *gmenu2x);

#endif
