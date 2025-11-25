#include "omx_main.h"
#include "globals.h"
#include "utils/omx_storage_mgr.h"
#include "modes/omx_mode_mgr.h"
#include "hardware/omx_inputs.h"
#include "hardware/omx_disp.h"
#include "hardware/omx_leds.h"
#include "utils/omx_util.h"
#include "utils/cvNote_util.h"
#include "midi/midi.h"
#include "midi/sysex.h"
#include "modes/sequencer.h"
#include "ClearUI/ClearUI.h"
#include "modes/omx_mode_midi_keyboard.h"
#include "modes/omx_mode_drum.h"
#include "modes/omx_mode_sequencer.h"
#include "modes/omx_mode_grids.h"
#include "modes/omx_mode_euclidean.h"
#include "modes/omx_mode_chords.h"
#include "modes/omx_screensaver.h"

// #define RAM_MONITOR
#ifdef RAM_MONITOR
#include "utils/RamMonitor.h"
RamMonitor ram;
uint32_t reporttime;

void report_ram_stat(const char *aname, uint32_t avalue)
{
	Serial.print(aname);
	Serial.print(": ");
	Serial.print((avalue + 512) / 1024);
	Serial.print(" Kb (");
	Serial.print((((float)avalue) / ram.total()) * 100, 1);
	Serial.println("%)");
};

void report_profile_time(const char *aname, uint32_t avalue)
{
	Serial.print(aname);
	Serial.print(": ");
	Serial.print(avalue);
	Serial.println("\n");
};

void report_ram()
{
	bool lowmem;
	bool crash;

	Serial.println("==== memory report ====");

	report_ram_stat("free", ram.adj_free());
	report_ram_stat("stack", ram.stack_total());
	report_ram_stat("heap", ram.heap_total());

	lowmem = ram.warning_lowmem();
	crash = ram.warning_crash();
	if (lowmem || crash)
	{
		Serial.println();

		if (crash)
			Serial.println("**warning: stack and heap crash possible");
		else if (lowmem)
			Serial.println("**warning: unallocated memory running low");
	};

	Serial.println();
};
#endif

void OMXMain::setup()
{

#if BOARDTYPE == TEENSY4
// 	Serial.println("Teensy 4.0");
// 	Serial.println("DAC Start!");
	dac.begin(DAC_ADDR);

#elif BOARDTYPE == OMX2040
// 	Serial.println("RP2040");
	TinyUSBDevice.setManufacturerDescriptor(mfgstr);
	TinyUSBDevice.setProductDescriptor(prodstr);

	pinMode(REDLED, OUTPUT);	// RED LED
	pinMode(BLUELED, OUTPUT);	// BLUE LED
	digitalWrite(REDLED, LOW); 	// digitalWrite(REDLED, LOW);
	digitalWrite(BLUELED, HIGH);

	pinMode(FIVEVEN, OUTPUT); 		// 5v enable Pin
	digitalWrite(FIVEVEN, HIGH);	// Turn 5v enable ON

	pinMode(TXLED, OUTPUT); 	// TX
	pinMode(RXLED, INPUT); 	// RX

	digitalWrite(TXLED, LOW);
	digitalWrite(RXLED, LOW);
	Wire1.setSDA(I2C_SDA);		// i2c1 SDA
	Wire1.setSCL(I2C_SCL);		// i2c1 SCL

// 	Serial1.setRX(RXLED);
// 	Serial1.setTX(TXLED);

	dac.begin(DAC_ADDR, &Wire1);

	// Initialize WebUSB for connection notification, etc
 	setupWebUSB();

#else
// 	Serial.println("Teensy 3.2");
#endif

	// HW MIDI
	MM::begin();

	// CV GATE pin
	pinMode(CVGATE_PIN, OUTPUT);
	// ENCODER BUTTON pin
	pinMode(buttonPin, INPUT_PULLUP);

	// Storage - FIX?
	storage = Storage::initStorage();
	sysEx = new SysEx(storage, &sysSettings);
	// Serial.println( "initStorage" );

#ifdef RAM_MONITOR
	ram.initialize();
#endif

	// clksTimer = 0; // TODO - didn't see this used anywhere
	omxScreensaver.resetCounter();
	// ssstep = 0;

	lastProcessTime = micros();
	omxUtil.restartClocks();
	omxUtil.subModeClearStorage.setStoragePtr(storage);


// #if BOARDTYPE == OMX2040
	// while (!TinyUSBDevice.mounted()){
	// 	delay(100);
	// }
// #endif

	// Serial
	Serial.begin(115200);
	delay(100);


	// SET ANALOG READ resolution to teensy's 13 usable bits
#if BOARDTYPE == TEENSY4
	randomSeed(analogRead(13));
	srand(analogRead(13));
	analogReadResolution(10); // Teensy 4 = 10 bits
#elif BOARDTYPE == OMX2040
// 	randomSeed(analogRead(29));
// 	srand(analogRead(29));
	analogReadResolution(10); // MUX = 10 bits
#else
	randomSeed(analogRead(13));
	srand(analogRead(13));
	analogReadResolution(13); // Teensy 3.x = 13 bits
#endif


	// initialize ANALOG INPUTS and ResponsiveAnalogRead
	for (int i = 0; i < potCount; i++)
	{
// 		potSettings.analog[i] = new ResponsiveAnalogRead(0, true, .001);
// 		potSettings.analog[i]->setAnalogResolution(1 << 13);

#if BOARDTYPE == TEENSY4
		pinMode(analogPins[i], INPUT);
		potSettings.analog[i] = new ResponsiveAnalogRead(analogPins[i], true, .001);
// 		potSettings.analog[i]->setAnalogResolution(10);
//		potSettings.analog[i]->setActivityThreshold(8);
#elif BOARDTYPE == OMX2040
		potSettings.analog[i] = new ResponsiveAnalogRead(mux_common_pin, true, .001);

#else
		pinMode(analogPins[i], INPUT);
		potSettings.analog[i] = new ResponsiveAnalogRead(analogPins[i], true, .001);
		potSettings.analog[i]->setAnalogResolution(1 << 13);
		potSettings.analog[i]->setActivityThreshold(32);
#endif

		currentValue[i] = 0;
		lastMidiValue[i] = 0;
	}

	// set DAC Resolution CV/GATE
	RES = 12;
	AMAX = pow(2, RES);
	V_scale = 64; // pow(2,(RES-7)); 4095 max

#if BOARDTYPE == TEENSY4
	dac.setVoltage(0, false);
#elif BOARDTYPE == OMX2040
	dac.setVoltage(0, false);
#else
	analogWriteResolution(RES); // set resolution for DAC
	analogWrite(CVPITCH_PIN, 0);
#endif

	globalScale.calculateScale(scaleConfig.scaleRoot, scaleConfig.scalePattern);
	omxModeMidi.SetScale(&globalScale);
	omxModeDrum.SetScale(&globalScale);
	omxModeSeq.SetScale(&globalScale);

#ifdef OMXMODEGRIDS
	omxModeGrids.SetScale(&globalScale);
#endif
	omxModeEuclid.SetScale(&globalScale);
	omxModeChords.SetScale(&globalScale);

	// Keypad
	//	customKeypad.begin();
	keypad.begin();
	// Serial.println( "Init keypad" );

	// Init Display
	omxDisp.setup();

	// Startup screen
	omxDisp.drawStartupScreen();

	// LEDs
	omxLeds.initSetup();
	// Serial.println( "Init LEDs" );


	// Load settings from EEPROM
	// bool bLoaded = false; // loadFromStorage();
	// Serial.println( "Load from EEPROM" );
	bool bLoaded = OmxStorageMgr::loadFromStorage();

	if (!bLoaded)
	{
		// Serial.println( "Init load fail. Reinitializing" );

		// Failed to load due to initialized EEPROM or version mismatch
		// defaults
		sequencer.playingPattern = 0;
		sysSettings.playingPattern = 0;
		sysSettings.midiChannel = 1;
		pots[0][0] = CC1;
		pots[0][1] = CC2;
		pots[0][2] = CC3;
		pots[0][3] = CC4;
		pots[0][4] = CC5;

		omxModeSeq.initPatterns();

		OmxModeMgr::changeOmxMode(DEFAULT_MODE);
		// initPatterns();
		OmxStorageMgr::saveToStorage();
	}


#ifdef RAM_MONITOR
	reporttime = millis();
#endif


}

void OMXMain::loop()
{
	//	customKeypad.tick();
	keypad.tick();
	// clksTimer = 0; // TODO - didn't see this used anywhere

	Micros now = micros();
	Micros passed = now - lastProcessTime;
	lastProcessTime = now;

	sysSettings.timeElasped = passed;

	seqConfig.currentFrameMicros = micros();
	// Micros timeStart = micros();
	activeOmxMode->loopUpdate(passed);
	cvNoteUtil.loopUpdate(passed);

	if (passed > 0) // This should always be true
	{
		if (sequencer.playing || omxUtil.areClocksRunning())
		{
			omxScreensaver.resetCounter(); // screenSaverCounter = 0;
		}
		omxUtil.advanceClock(activeOmxMode, passed);
		omxUtil.advanceSteps(passed);
	}

	// DISPLAY SETUP -- why is this display. instead of omxDisp. ??
	display.clearDisplay();

	// ############### SLEEP MODE ###############
	//
	//	Serial.println(screenSaverCounter);
	omxScreensaver.updateScreenSaverState();
	sysSettings.screenSaverMode = omxScreensaver.shouldShowScreenSaver();

	// ############### POTS ###############
	//
	OmxInputs::readPotentimeters();

	bool omxModeChangedThisFrame = false;

	// ############### EXTERNAL MODE CHANGE / SYSEX ###############
	if ((!encoderConfig.enc_edit && (sysSettings.omxMode != sysSettings.newmode)) || sysSettings.refresh)
	{
		sysSettings.newmode = sysSettings.omxMode;
		OmxModeMgr::changeOmxMode(sysSettings.omxMode);
		omxModeChangedThisFrame = true;

		sequencer.playingPattern = sysSettings.playingPattern;
		omxDisp.setDirty();
		omxLeds.setAllLEDS(0, 0, 0);
		omxLeds.setDirty();
		sysSettings.refresh = false;
	}

	// ############### ENCODER ###############
	//
	auto u = myEncoder.update();
// 	Serial.println("Encoder update");
	if (u.active())
	{
		auto amt = u.accel(1);		   // where 5 is the acceleration factor if you want it, 0 if you don't)
		omxScreensaver.resetCounter(); // screenSaverCounter = 0;
									   //    	Serial.println(u.dir() < 0 ? "ccw " : "cw ");
									   //    	Serial.println(amt);

		// Change Mode
		if (encoderConfig.enc_edit)
		{
			// set mode
			//			int modesize = NUM_OMX_MODES;
			sysSettings.newmode = (OMXMode)constrain(sysSettings.newmode + amt, 0, NUM_OMX_MODES - 1);
			// omxDisp.dispMode();
			// omxDisp.bumpDisplayTimer();
			omxDisp.setDirty();
			omxLeds.setDirty();
		}
		else
		{
			activeOmxMode->onEncoderChanged(u);
		}
	}
	// END ENCODER

	// ############### ENCODER BUTTON ###############
	//
	auto s = encButton.update();
	switch (s)
	{
	// SHORT PRESS
	case Button::Down:				   // Serial.println("Button down");
		omxScreensaver.resetCounter(); // screenSaverCounter = 0;

		// what page are we on?
		if (sysSettings.newmode != sysSettings.omxMode && encoderConfig.enc_edit)
		{
			OmxModeMgr::changeOmxMode(sysSettings.newmode);
			omxModeChangedThisFrame = true;
			seqStop();
			omxLeds.setAllLEDS(0, 0, 0);
			encoderConfig.enc_edit = false;
			// omxDisp.dispMode();
			omxDisp.setDirty();
		}
		else if (encoderConfig.enc_edit)
		{
			encoderConfig.enc_edit = false;
		}

		// Prevents toggling encoder select when entering mode
		if (!omxModeChangedThisFrame)
		{
			activeOmxMode->onEncoderButtonDown();
		}

		omxDisp.setDirty();
		break;

	// LONG PRESS
	case Button::DownLong: // Serial.println("Button downlong");
		if (activeOmxMode->shouldBlockEncEdit())
		{
			activeOmxMode->onEncoderButtonDown();
		}
		else
		{
			// Enter mode change
			encoderConfig.enc_edit = true;
			sysSettings.newmode = sysSettings.omxMode;
			omxLeds.setAllLEDS(0, 0, 0);
			omxDisp.setDirty();
			// omxDisp.dispMode();
		}

		omxDisp.setDirty();
		break;
	case Button::Up: // Serial.println("Button up");
		activeOmxMode->onEncoderButtonUp();
		break;
	case Button::UpLong: // Serial.println("Button uplong");
		activeOmxMode->onEncoderButtonUpLong();
		break;
	default:
		break;
	}
	// END ENCODER BUTTON

	// ############### KEY HANDLING ###############
	//
	while (keypad.available())
	{
// 		Serial.println("keypad");
		auto e = keypad.next();
		int thisKey = e.key();
		bool keyConsumed = false;
		// int keyPos = thisKey - 11;
		// int seqKey = keyPos + (sequencer.patternPage[sequencer.playingPattern] * NUM_STEPKEYS);

		if (e.down())
		{
			omxScreensaver.resetCounter(); // screenSaverCounter = 0;
			midiSettings.keyState[thisKey] = true;
		}

		if (e.down() && thisKey == 0 && encoderConfig.enc_edit)
		{
			// temp - save whenever the 0 key is pressed in encoder edit mode
			omxDisp.displayMessage("Saving...");
			omxDisp.isDirty();
			omxDisp.showDisplay();
			OmxStorageMgr::saveToStorage();
			//	Serial.println("EEPROM saved");
			omxDisp.displayMessage("Saved State");
			encoderConfig.enc_edit = false;
			omxLeds.setAllLEDS(0, 0, 0);
			activeOmxMode->onModeActivated();
			omxDisp.isDirty();
			omxLeds.isDirty();
			keyConsumed = true;
		}

		if (!keyConsumed)
		{
			activeOmxMode->onKeyUpdate(e);
		}

		// END MODE SWITCH

		if (!e.down())
		{
			midiSettings.keyState[thisKey] = false;
		}

		// ### LONG KEY SWITCH PRESS
		if (e.held() && !keyConsumed)
		{
			// DO LONG PRESS THINGS
			activeOmxMode->onKeyHeldUpdate(e); // Only the sequencer uses this, could probably be handled in onKeyUpdate() but keyStates are modified before this stuff happens.
		}									   // END IF HELD

	} // END KEYS WHILE

	if (!sysSettings.screenSaverMode)
	{
		omxLeds.updateBlinkStates();
		omxDisp.UpdateMessageTextTimer();

		if (encoderConfig.enc_edit)
		{
			omxDisp.dispMode();
		}
		else
		{
			activeOmxMode->onDisplayUpdate();
		}
	}
	else
	{ // if screenSaverMode
		omxScreensaver.onDisplayUpdate();
	}

	// DISPLAY at end of loop
	omxDisp.showDisplay();
	omxLeds.showLeds();

	while (MM::usbMidiRead())
	{
		// incoming messages - see handlers
	}
	while (MM::midiRead())
	{
		// incoming messages - see handlers
	}

	// Serial.println(clockstats.getBPM());

	// Micros elapsed = micros() - timeStart;
	// if ((timeStart - reporttime) > 2000)
	// {
	// 	report_profile_time("Elapsed", elapsed);
	// 	reporttime = timeStart;
	// 	// report_ram();
	// };

#ifdef RAM_MONITOR
	uint32_t time = millis();

	if ((time - reporttime) > 2000)
	{
		reporttime = time;
		report_ram();
	};

	ram.run();
#endif

}
