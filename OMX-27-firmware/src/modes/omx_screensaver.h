#pragma once

#include "./omx_mode_interface.h"

class OmxScreensaver : public OmxModeInterface
{
public:
	OmxScreensaver() {}
	~OmxScreensaver() {}
	const char *modeName = "Screensaver";

	char* getModeName() override{return (char*)modeName;}

	void onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta) override;

	void updateLEDs() override;

	void resetCounter();

	void updateScreenSaverState();
	void setScreenSaverColor();
	bool shouldShowScreenSaver();

	void onEncoderChanged(Encoder::Update enc) override;

	void onEncoderButtonDown() override{};
	void onEncoderButtonDownLong() override{};

	void onKeyUpdate(OMXKeypadEvent e) override;
	void onKeyHeldUpdate(OMXKeypadEvent e){};

	void onDisplayUpdate() override;

private:
	elapsedMillis screenSaverCounter = 0;
	unsigned long screensaverInterval = 1000 * 60 * 3; // 3 minutes default? // 10000;  15000; //

	// Uncomment for 10 second screensaver for testing
	// unsigned long screensaverInterval = 1000 * 10;
	int ssstep = 0;
	int ssloop = 0;
	volatile unsigned long nextStepTimeSS = 0;
	bool ssreverse = false;
	int sleepTick = 80;
	bool screenSaverActive;
};
