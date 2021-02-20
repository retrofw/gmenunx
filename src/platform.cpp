#include "platform.h"
#include "platform/retrofw.h"
#include "platform/opendingux.h"
#include "platform/miyoo.h"
// #include "platform/gp2x.h"

Platform::Platform(GMenu2X *gmenu2x) : gmenu2x(gmenu2x) {
	WARNING("Linux");
}

uint16_t Platform::getDevStatus() {
	char buf[10000];
	if (FILE *f = fopen("/proc/bus/input/devices", "r")) {
	// if (f = fopen("/proc/bus/input/handlers", "r")) {
		size_t sz = fread(buf, sizeof(char), 10000, f);
		fclose(f);
		return sz;
	}
	return 0;
}

uint32_t Platform::hwCheck(unsigned int interval, void *param) {
	printf("%s:%d: %s\n", __FILE__, __LINE__, __func__);
	numJoy = getDevStatus();
	if (numJoyPrev != numJoy) {
		numJoyPrev = numJoy;
		InputManager::pushEvent(JOYSTICK_CONNECT);
	}

	return 0;
}

Platform* PlatformInit(GMenu2X *gmenu2x) { // Detect platform type and return base class pointer
	if (file_exists("/proc/jz/gpio")) {
		return new RetroFW(gmenu2x);
	}

	if (file_exists("/sys/devices/platform/jz-lcd.0/keep_aspect_ratio")) {
		return new OpenDingux(gmenu2x);
	}

	if (file_exists("/sys/devices/platform/soc/1c23400.battery/power_supply/miyoo-battery/voltage_now")) {
		return new Miyoo(gmenu2x);
	}

	return new Platform(gmenu2x);
}
