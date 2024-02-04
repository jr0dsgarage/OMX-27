#include "omx_screensaver.h"
#include "../consts/consts.h"
#include "../config.h"
#include "../utils/omx_util.h"
#include "../hardware/omx_disp.h"
#include "../hardware/omx_leds.h"

void OmxScreensaver::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
{
	setScreenSaverColor();

	// reset screensaver
	if (potSettings.analog[0]->hasChanged() || potSettings.analog[1]->hasChanged() || potSettings.analog[2]->hasChanged() || potSettings.analog[3]->hasChanged())
	{
		screenSaverCounter = 0;
	}
}

void OmxScreensaver::updateScreenSaverState()
{
	if (screenSaverCounter > screensaverInterval)
	{
		screenSaverActive = true;
	}
	else if (screenSaverCounter < 10)
	{
		ssstep = 0;
		ssloop = 0;
		//		setAllLEDS(0,0,0);
		screenSaverActive = false;
		nextStepTimeSS = millis();
	}
	else
	{
		screenSaverActive = false;
		nextStepTimeSS = millis();
	}
}

void OmxScreensaver::setScreenSaverColor()
{
	// full color range is 0-65528, but its reduced here to avoid red color duplication, so it becomes 0-62613
	colorConfig.screensaverColor = map(potSettings.analog[4]->getValue(), potMinVal, potMaxVal, 0, 62613);
}

bool OmxScreensaver::shouldShowScreenSaver()
{
	return screenSaverActive;
}

void OmxScreensaver::onEncoderChanged(Encoder::Update enc)
{
	// Add debug statements to troubleshoot the code
	Serial.println("onEncoderChanged function called.");
	Serial.print("Encoder active? ");
	Serial.println(enc.active());
	Serial.print("Encoder direction: ");
	Serial.println(enc.dir());
}

void OmxScreensaver::onKeyUpdate(OMXKeypadEvent e)
{
}

void OmxScreensaver::onDisplayUpdate()
{
	updateLEDs();
	omxDisp.clearDisplay();
}
void OmxScreensaver::resetCounter()
{
	screenSaverCounter = 0;
}
void OmxScreensaver::updateLEDs()
{
	unsigned long playstepmillis = millis();
	if (playstepmillis > nextStepTimeSS)
	{
		ssstep = ssstep % 16;
		ssloop = ssloop % 16;

		int j = 26 - ssloop;
		int i = ssstep + 11;

		for (int z = 1; z < 11; z++)
		{
			strip.setPixelColor(z, 0);
		}
		if (colorConfig.screensaverColor != 0)
		{
			if (!ssreverse)
			{
				// turn off all leds
				for (int x = 0; x < 16; x++)
				{
					if (i < j)
					{
						strip.setPixelColor(x + 11, 0);
					}
					if (x + 11 > j)
					{
						strip.setPixelColor(x + 11, strip.gamma32(strip.ColorHSV(colorConfig.screensaverColor)));
					}
				}
				strip.setPixelColor(i + 1, strip.gamma32(strip.ColorHSV(colorConfig.screensaverColor)));
			}
			else
			{
				for (int y = 0; y < 16; y++)
				{
					if (i >= j)
					{
						strip.setPixelColor(y + 11, 0);
					}
					if (y + 11 < j)
					{
						strip.setPixelColor(y + 11, strip.gamma32(strip.ColorHSV(colorConfig.screensaverColor)));
					}
				}
				strip.setPixelColor(i + 1, strip.gamma32(strip.ColorHSV(colorConfig.screensaverColor)));
			}
		}
		else
		{
			for (int w = 0; w < 27; w++)
			{
				strip.setPixelColor(w, 0);
			}
		}
		ssstep++;
		if (ssstep == 16)
		{
			ssloop++;
		}
		if (ssloop == 16)
		{
			ssreverse = !ssreverse;
		}
		nextStepTimeSS = nextStepTimeSS + sleepTick;

		omxLeds.setDirty();
	}
}
