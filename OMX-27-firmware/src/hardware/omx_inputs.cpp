#include "omx_inputs.h"
#include "../globals.h"
#include "../modes/omx_screensaver.h"
#include "../modes/omx_mode_interface.h"

void OmxInputs::readPotentimeters()
{
    int temp;

	for (int k = 0; k < potCount; k++)
	{
		int prevValue = potSettings.analogValues[k];
		int prevAnalog = potSettings.analog[k]->getValue();
#if BOARDTYPE == OMX2040
		temp = mux.read(muxMapping[k]);
// 		temp = 0;
#else
		temp = analogRead(analogPins[k]);
#endif
		potSettings.analog[k]->update(temp);
		// read from the smoother, constrain (to account for tolerances), and map it
		temp = potSettings.analog[k]->getValue();
		temp = constrain(temp, potMinVal, potMaxVal);
		temp = map(temp, potMinVal, potMaxVal, 0, 16383);
		potSettings.hiResPotVal[k] = temp;

		// map and update the value
		potSettings.analogValues[k] = temp >> 7;

		int newAnalog = potSettings.analog[k]->getValue();

		// delta is way smaller on T4 - what to do??
		int analogDelta = abs(newAnalog - prevAnalog);

		// if (k == 1)
		// {
		// 	Serial.print(analogPins[k]);
		// 	Serial.print(" ");
		// 	Serial.print(temp);
		// 	Serial.print(" ");
		// 	Serial.print(potSettings.analogValues[k]);
		// 	Serial.print("\n");
		// }

		if (potSettings.analog[k]->hasChanged())
		{
			// do stuff
			if (sysSettings.screenSaverMode)
			{
				omxScreensaver.onPotChanged(k, prevValue, potSettings.analogValues[k], analogDelta);
			}
			// don't send pots in screensaver
			else
			{
				activeOmxMode->onPotChanged(k, prevValue, potSettings.analogValues[k], analogDelta);
			}
		}

	}
}
