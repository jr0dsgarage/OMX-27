#include "omx_mode_drum.h"
#include "../config.h"
#include "../consts/colors.h"
#include "../utils/omx_util.h"
#include "../utils/cvNote_util.h"
#include "../hardware/omx_disp.h"
#include "../hardware/omx_leds.h"
#include "../midi/midi.h"
#include "../utils/music_scales.h"
#include "../midi/noteoffs.h"

enum DrumModePage {
    DRUMPAGE_DRUMKEY, // Note, Chan, Vel, MidiFX
    DRUMPAGE_DRUMKEY2, // Hue, RND Hue, Copy, Paste
    DRUMPAGE_SCALES, // Hue,
    DRUMPAGE_INSPECT, // Sent Pot CC, Last Note, Last Vel, Last Chan, Not editable, just FYI
    DRUMPAGE_POTSANDMACROS, // PotBank, Thru, Macro, Macro Channel
    DRUMPAGE_CFG,
    DRUMPAGE_NUMPAGES
};

enum DrumEditMode {
    DRUMMODE_NORMAL,
    DRUMMODE_LOADKIT,
    DRUMMODE_SAVEKIT,
    DRUMMODE_NUM_OF_MODES,
};

OmxModeDrum::OmxModeDrum()
{
    params.addPages(DRUMPAGE_NUMPAGES);

	m8Macro_.setDoNoteOn(&OmxModeDrum::doNoteOnForwarder, this);
	m8Macro_.setDoNoteOff(&OmxModeDrum::doNoteOffForwarder, this);
	nornsMarco_.setDoNoteOn(&OmxModeDrum::doNoteOnForwarder, this);
	nornsMarco_.setDoNoteOff(&OmxModeDrum::doNoteOffForwarder, this);
	delugeMacro_.setDoNoteOn(&OmxModeDrum::doNoteOnForwarder, this);
	delugeMacro_.setDoNoteOff(&OmxModeDrum::doNoteOffForwarder, this);

    presetManager.setContextPtr(this);
    presetManager.setDoSaveFunc(&OmxModeDrum::doSaveKitForwarder);
    presetManager.setDoLoadFunc(&OmxModeDrum::doLoadKitForwarder);
}

void OmxModeDrum::changeMode(uint8_t newModeIndex)
{
    if(newModeIndex >= DRUMMODE_NUM_OF_MODES)
    {
        return;
    }

    if(newModeIndex == DRUMMODE_NORMAL)
    {
        disableSubmode();
    }
    if(newModeIndex == DRUMMODE_SAVEKIT)
    {
        presetManager.configure(PRESETMODE_SAVE, selDrumKit, NUM_DRUM_KITS, true);
        enableSubmode(&presetManager);
    }
    else if(newModeIndex == DRUMMODE_LOADKIT)
    {
        presetManager.configure(PRESETMODE_LOAD, selDrumKit, NUM_DRUM_KITS, true);
        enableSubmode(&presetManager);
    }
}


void OmxModeDrum::InitSetup()
{
	initSetup = true;
}

void OmxModeDrum::onModeActivated()
{
	// auto init when activated
	if (!initSetup)
	{
		InitSetup();
	}

	// sequencer.playing = false;
	stopSequencers();

	omxLeds.setDirty();
	omxDisp.setDirty();

	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		subModeMidiFx[i].setEnabled(true);
		subModeMidiFx[i].onModeChanged();
		subModeMidiFx[i].setNoteOutputFunc(&OmxModeDrum::onNotePostFXForwarder, this);
	}

	pendingNoteOffs.setNoteOffFunction(&OmxModeDrum::onPendingNoteOffForwarder, this);

	params.setSelPageAndParam(0, 0);
	encoderSelect = true;

    activeDrumKit.CopyFrom(drumKits[selDrumKit]);

	// selectMidiFx(mfxIndex_, false);
}

void OmxModeDrum::onModeDeactivated()
{
	stopSequencers();

	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		subModeMidiFx[i].setEnabled(false);
		subModeMidiFx[i].onModeChanged();
	}
}

void OmxModeDrum::stopSequencers()
{
	omxUtil.stopClocks();
	pendingNoteOffs.allOff();
}

void OmxModeDrum::selectMidiFx(uint8_t mfxIndex, bool dispMsg)
{
    uint8_t prevMidiFX = activeDrumKit.drumKeys[selDrumKey].midifx;
    
    if(mfxIndex != prevMidiFX && prevMidiFX < NUM_MIDIFX_GROUPS)
    {
        drumKeyUp(selDrumKey + 1);
    }

    activeDrumKit.drumKeys[selDrumKey].midifx = mfxIndex;

	if(mfxQuickEdit_)
	{
		// Change the MidiFX Group being edited
		if(mfxIndex < NUM_MIDIFX_GROUPS && mfxIndex != quickEditMfxIndex_)
		{
			enableSubmode(&subModeMidiFx[mfxIndex]);
			subModeMidiFx[mfxIndex].enablePassthrough();
			quickEditMfxIndex_ = mfxIndex;
			dispMsg = false;
		}
		else if(mfxIndex >= NUM_MIDIFX_GROUPS)
		{
			disableSubmode();
		}
	}

	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		subModeMidiFx[i].setSelected(i == mfxIndex);
	}

	if (dispMsg)
	{
		if (mfxIndex < NUM_MIDIFX_GROUPS)
		{
			omxDisp.displayMessageTimed("MidiFX " + String(mfxIndex + 1), 5);
		}
		else
		{
			omxDisp.displayMessageTimed("MidiFX Off", 5);
		}
	}
}

void OmxModeDrum::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
{
	if (isSubmodeEnabled() && activeSubmode->usesPots())
	{
		activeSubmode->onPotChanged(potIndex, prevValue, newValue, analogDelta);
		return;
	}

	auto activeMacro = getActiveMacro();

	bool macroConsumesPots = false;
	if (activeMacro != nullptr)
	{
		macroConsumesPots = activeMacro->consumesPots();
	}

	// Note, these get sent even if macro mode is not active
	if (macroConsumesPots)
	{
		activeMacro->onPotChanged(potIndex, prevValue, newValue, analogDelta);
	}
	else
	{
		omxUtil.sendPots(potIndex, sysSettings.midiChannel);
	}

	omxDisp.setDirty();
}

void OmxModeDrum::onClockTick()
{
	for (uint8_t i = 0; i < 5; i++)
	{
		// Lets them do things in background
		subModeMidiFx[i].onClockTick();
	}
}

void OmxModeDrum::loopUpdate(Micros elapsedTime)
{
	for (uint8_t i = 0; i < 5; i++)
	{
		// Lets them do things in background
		subModeMidiFx[i].loopUpdate();
	}

	// Can be modified by scale MidiFX
	musicScale->calculateScaleIfModified(scaleConfig.scaleRoot, scaleConfig.scalePattern);
	
}

bool OmxModeDrum::isDrumKeyHeld()
{
	if(isSubmodeEnabled()) return false;

	for(uint8_t i = 1; i < 27; i++)
	{
		if(midiSettings.midiKeyState[i] >= 0)
		{
			return true;
		}
	}

	return false;
}

bool OmxModeDrum::getEncoderSelect()
{
	return encoderSelect && !midiSettings.midiAUX && !isDrumKeyHeld();
}

void OmxModeDrum::onEncoderChanged(Encoder::Update enc)
{
	if (isSubmodeEnabled())
	{
		activeSubmode->onEncoderChanged(enc);
		return;
	}

	bool macroConsumesDisplay = false;

	if (macroActive_ && activeMacro_ != nullptr)
	{
		macroConsumesDisplay = activeMacro_->consumesDisplay();
	}

	if (macroConsumesDisplay)
	{
		activeMacro_->onEncoderChanged(enc);
		return;
	}

	if (getEncoderSelect())
	{
		// onEncoderChangedSelectParam(enc);
		params.changeParam(enc.dir());
		omxDisp.setDirty();
		return;
	}

	auto amt = enc.accel(5); // where 5 is the acceleration factor if you want it, 0 if you don't)

	int8_t selPage = params.getSelPage(); 
	int8_t selParam = params.getSelParam() + 1; // Add one for readability

	if (selPage == DRUMPAGE_DRUMKEY)
	{
        auto drumKey = activeDrumKit.drumKeys[selDrumKey];
        
		if (selParam == 1) // NoteNum
		{
            drumKey.noteNum = constrain(drumKey.noteNum + amt, 0, 127);
		}
		else if (selParam == 2) // Chan
		{
            drumKey.chan = constrain(drumKey.chan + amt, 1, 16);
		}
		else if (selParam == 3) // Vel
		{
            drumKey.vel = constrain(drumKey.vel + amt, 0, 127);
		}
        else if (selParam == 4) // MidiFX Slot
		{
            int midiFX = drumKey.midifx;
            if(midiFX > NUM_MIDIFX_GROUPS)
            {
                midiFX = -1;
            }
            midiFX = constrain(midiFX + amt, -1, NUM_MIDIFX_GROUPS - 1);
            if(midiFX < 0)
            {
                midiFX = 127;
            }
            drumKey.midifx = midiFX;
            selectMidiFx(midiFX, false);
		}

        // Apply changes
        activeDrumKit.drumKeys[selDrumKey] = drumKey;
	}
    else if (selPage == DRUMPAGE_DRUMKEY2)
	{
        auto drumKey = activeDrumKit.drumKeys[selDrumKey];
        
		if (selParam == 1) // Hue
		{
            drumKey.hue = constrain(drumKey.hue + amt, 0, 255);
            omxLeds.setDirty();
		}

        // Apply changes
        activeDrumKit.drumKeys[selDrumKey] = drumKey;
	}
	else if (selPage == DRUMPAGE_POTSANDMACROS)
	{
		if (selParam == 1)
		{
			potSettings.potbank = constrain(potSettings.potbank + amt, 0, NUM_CC_BANKS - 1);
		}
		if (selParam == 2)
		{
			midiSettings.midiSoftThru = constrain(midiSettings.midiSoftThru + amt, 0, 1);
		}
		if (selParam == 3)
		{
			midiMacroConfig.midiMacro = constrain(midiMacroConfig.midiMacro + amt, 0, nummacromodes);
		}
		if (selParam == 4)
		{
			midiMacroConfig.midiMacroChan = constrain(midiMacroConfig.midiMacroChan + amt, 1, 16);
		}
	}
    else if (selPage == DRUMPAGE_SCALES)
	{
		if (selParam == 1)
		{
			int prevRoot = scaleConfig.scaleRoot;
			scaleConfig.scaleRoot = constrain(scaleConfig.scaleRoot + amt, 0, 12 - 1);
			if (prevRoot != scaleConfig.scaleRoot)
			{
				musicScale->calculateScale(scaleConfig.scaleRoot, scaleConfig.scalePattern);
			}
		}
		if (selParam == 2)
		{
			int prevPat = scaleConfig.scalePattern;
			scaleConfig.scalePattern = constrain(scaleConfig.scalePattern + amt, -1, musicScale->getNumScales() - 1);
			if (prevPat != scaleConfig.scalePattern)
			{
				omxDisp.displayMessage(musicScale->getScaleName(scaleConfig.scalePattern));
				musicScale->calculateScale(scaleConfig.scaleRoot, scaleConfig.scalePattern);
			}
		}
		if (selParam == 3)
		{
			scaleConfig.lockScale = constrain(scaleConfig.lockScale + amt, 0, 1);
		}
		if (selParam == 4)
		{
			scaleConfig.group16 = constrain(scaleConfig.group16 + amt, 0, 1);
		}
	}
	else if(selPage == DRUMPAGE_CFG)
	{
		if (selParam == 3)
		{
			clockConfig.globalQuantizeStepIndex = constrain(clockConfig.globalQuantizeStepIndex + amt, 0, kNumArpRates - 1);
		}
		else if (selParam == 4)
		{
			cvNoteUtil.triggerMode = constrain(cvNoteUtil.triggerMode + amt, 0, 1);
		}
	}

	omxDisp.setDirty();
}

void OmxModeDrum::onEncoderButtonDown()
{
	if (isSubmodeEnabled())
	{
		activeSubmode->onEncoderButtonDown();
		return;
	}

	bool macroConsumesDisplay = false;
	if (macroActive_ && activeMacro_ != nullptr)
	{
		macroConsumesDisplay = activeMacro_->consumesDisplay();
	}

	if (macroConsumesDisplay)
	{
		activeMacro_->onEncoderButtonDown();
		return;
	}

	if (params.getSelPage() == DRUMPAGE_DRUMKEY2)
	{
		auto selParam = params.getSelParam();

		if (selParam == 1)
		{
			randomizeHues();
			omxDisp.isDirty();
			omxLeds.isDirty();
			return;
		}
		else if (selParam == 2) // Copy
		{
			tempDrumKey.CopyFrom(activeDrumKit.drumKeys[selDrumKey]);
			omxDisp.displayMessage("Copied " + String(selDrumKey + 1));
			omxDisp.isDirty();
			omxLeds.isDirty();
			return;
		}
		else if (selParam == 3) // Paste
		{
			activeDrumKit.drumKeys[selDrumKey].CopyFrom(tempDrumKey);
			omxDisp.displayMessage("Pasted " + String(selDrumKey + 1));
			omxDisp.isDirty();
			omxLeds.isDirty();
			return;
		}
	}

	if (params.getSelPage() == DRUMPAGE_CFG && params.getSelParam() == 0)
	{
		enableSubmode(&subModePotConfig_);
		omxDisp.isDirty();
		return;
	}

	encoderSelect = !encoderSelect;
	omxDisp.isDirty();
}

void OmxModeDrum::onEncoderButtonUp()
{
}

void OmxModeDrum::onEncoderButtonDownLong()
{
}

bool OmxModeDrum::shouldBlockEncEdit()
{
	if (isSubmodeEnabled())
	{
		return activeSubmode->shouldBlockEncEdit();
	}

	if (macroActive_)
	{
		return true;
	}

	return false;
}

void OmxModeDrum::saveKit(uint8_t saveIndex)
{
    drumKits[saveIndex].CopyFrom(activeDrumKit);
    selDrumKit = saveIndex;
}

void OmxModeDrum::loadKit(uint8_t loadIndex)
{
    activeDrumKit.CopyFrom(drumKits[loadIndex]);
    selDrumKit = loadIndex;
}

void OmxModeDrum::onKeyUpdate(OMXKeypadEvent e)
{
	if (isSubmodeEnabled())
	{
		if (activeSubmode->onKeyUpdate(e))
			return;
	}

	int thisKey = e.key();

	// Aux double click toggle macro
	if (!isSubmodeEnabled() && midiMacroConfig.midiMacro > 0)
	{
		if (!macroActive_)
		{
			// Enter M8 Mode
			if (!e.down() && thisKey == 0 && e.clicks() == 2)
			{
				midiSettings.midiAUX = false;

				activeMacro_ = getActiveMacro();
				if (activeMacro_ != nullptr)
				{
					macroActive_ = true;
					activeMacro_->setEnabled(true);
					activeMacro_->setScale(musicScale);
					omxLeds.setDirty();
					omxDisp.setDirty();
					return;
				}
				return;
			}
		}
		else // Macro mode active
		{
			if (!e.down() && thisKey == 0 && e.clicks() == 2)
			{
				// exit macro mode
				if (activeMacro_ != nullptr)
				{
					activeMacro_->setEnabled(false);
					activeMacro_ = nullptr;
				}

				midiSettings.midiAUX = false;
				macroActive_ = false;
				omxLeds.setDirty();
				omxDisp.setDirty();

				// Clear LEDs
				// for (int m = 1; m < LED_COUNT; m++)
				// {
				// 	strip.setPixelColor(m, LEDOFF);
				// }
			}
			else
			{
				if (activeMacro_ != nullptr)
				{
					activeMacro_->onKeyUpdate(e);
				}
			}
			return;
		}
	}

	if (onKeyUpdateSelMidiFX(e))
		return;

	// REGULAR KEY PRESSES
	if (!e.held())
	{ // IGNORE LONG PRESS EVENTS
		if (e.down() && thisKey != 0)
		{
			bool keyConsumed = false; // If used for aux, key will be consumed and not send notes.

			if (midiSettings.midiAUX) // Aux mode
			{
				keyConsumed = true;

				// if (thisKey == 11 || thisKey == 12) // Change Octave
				// {
				// 	int amt = thisKey == 11 ? -1 : 1;
				// 	midiSettings.octave = constrain(midiSettings.octave + amt, -5, 4);
				// }
				if (!mfxQuickEdit_ && (thisKey == 1 || thisKey == 2)) // Change Param selection
				{
					if (thisKey == 1)
					{
						params.decrementParam();
					}
					else if (thisKey == 2)
					{
						params.incrementParam();
					}
				}
                else if(thisKey == 3)
                {
                    changeMode(DRUMMODE_LOADKIT);
                    return;
                }
                else if(thisKey == 4)
                {
                    changeMode(DRUMMODE_SAVEKIT);
                    return;
                }
				else if (thisKey == 11 || thisKey == 12)
				{
					saveKit(selDrumKit);

					int8_t amt = thisKey == 11 ? -1 : 1;
					uint8_t newKitIndex = (selDrumKit + NUM_DRUM_KITS + amt) % NUM_DRUM_KITS;
					loadKit(newKitIndex); 

					omxDisp.displayMessage("Loaded " + String(newKitIndex + 1));
				}
			}

			if (!keyConsumed)
			{
				drumKeyDown(thisKey);
			}
		}
		else if (!e.down() && thisKey != 0)
		{
			drumKeyUp(thisKey);
		}
	}

	// AUX KEY
	if (e.down() && thisKey == 0)
	{
		if (!macroActive_)
		{
			midiSettings.midiAUX = true;
		}
	}
	else if (!e.down() && thisKey == 0)
	{
		if (midiSettings.midiAUX)
		{
			midiSettings.midiAUX = false;
		}
	}

	omxLeds.setDirty();
	omxDisp.setDirty();
}

bool OmxModeDrum::onKeyUpdateSelMidiFX(OMXKeypadEvent e)
{
	int thisKey = e.key();

	bool keyConsumed = false;

    uint8_t mfxIndex = activeDrumKit.drumKeys[selDrumKey].midifx;

	if (!e.held())
	{
        // Double Click to edit midi fx
		if (!e.down() && e.clicks() == 2 && thisKey >= 6 && thisKey < 11)
		{
			if (midiSettings.midiAUX) // Aux mode
			{
				enableSubmode(&subModeMidiFx[thisKey - 6]);
				keyConsumed = true;
			}
		}

		if (e.down() && thisKey != 0)
		{
			if (midiSettings.midiAUX) // Aux mode
			{
				if (mfxQuickEdit_ && thisKey == 1)
				{
					subModeMidiFx[quickEditMfxIndex_].selectPrevMFXSlot();
				}
				else if (mfxQuickEdit_ && thisKey == 2)
				{
					subModeMidiFx[quickEditMfxIndex_].selectNextMFXSlot();
				}
				else if (thisKey == 5)
				{
					keyConsumed = true;
					// Turn off midiFx
					selectMidiFx(127, true);
					// mfxIndex_ = 127;
				}
				else if (thisKey >= 6 && thisKey < 11)
				{
					keyConsumed = true;
					selectMidiFx(thisKey - 6, true);
					// Change active midiFx
					// mfxIndex_ = thisKey - 6;
				}
				else if (thisKey == 20) // MidiFX Passthrough
				{
					keyConsumed = true;
					if (mfxIndex < NUM_MIDIFX_GROUPS)
					{
						enableSubmode(&subModeMidiFx[mfxIndex]);
						subModeMidiFx[mfxIndex].enablePassthrough();
						mfxQuickEdit_ = true;
						quickEditMfxIndex_ = mfxIndex;
						midiSettings.midiAUX = false;
					}
					else
					{
						omxDisp.displayMessage(mfxOffMsg);
					}
				}
				else if (thisKey == 22) // Goto arp params
				{
					keyConsumed = true;
					if (mfxIndex < NUM_MIDIFX_GROUPS)
					{
						enableSubmode(&subModeMidiFx[mfxIndex]);
						subModeMidiFx[mfxIndex].gotoArpParams();
						midiSettings.midiAUX = false;
					}
					else
					{
						omxDisp.displayMessage(mfxOffMsg);
					}
				}
				else if (thisKey == 23) // Next arp pattern
				{
					keyConsumed = true;
					if (mfxIndex < NUM_MIDIFX_GROUPS)
					{
						subModeMidiFx[mfxIndex].nextArpPattern();
					}
					else
					{
						omxDisp.displayMessage(mfxOffMsg);
					}
				}
				else if (thisKey == 24) // Next arp octave
				{
					keyConsumed = true;
					if (mfxIndex < NUM_MIDIFX_GROUPS)
					{
						subModeMidiFx[mfxIndex].nextArpOctRange();
					}
					else
					{
						omxDisp.displayMessage(mfxOffMsg);
					}
				}
				else if (thisKey == 25)
				{
					keyConsumed = true;
					if (mfxIndex < NUM_MIDIFX_GROUPS)
					{
						subModeMidiFx[mfxIndex].toggleArpHold();

						if (subModeMidiFx[mfxIndex].isArpHoldOn())
						{
							omxDisp.displayMessageTimed("Arp Hold: On", 5);
						}
						else
						{
							omxDisp.displayMessageTimed("Arp Hold: Off", 5);
						}
					}
					else
					{
						omxDisp.displayMessage(mfxOffMsg);
					}
				}
				else if (thisKey == 26)
				{
					keyConsumed = true;
					if (mfxIndex < NUM_MIDIFX_GROUPS)
					{
						subModeMidiFx[mfxIndex].toggleArp();

						if (subModeMidiFx[mfxIndex].isArpOn())
						{
							omxDisp.displayMessageTimed("Arp On", 5);
						}
						else
						{
							omxDisp.displayMessageTimed("Arp Off", 5);
						}
					}
					else
					{
						omxDisp.displayMessage(mfxOffMsg);
					}
				}
			}
		}
	}

	return keyConsumed;
}

bool OmxModeDrum::onKeyHeldSelMidiFX(OMXKeypadEvent e)
{
	int thisKey = e.key();

	bool keyConsumed = false;

	if (midiSettings.midiAUX) // Aux mode
	{
		// Enter MidiFX mode
		if (thisKey >= 6 && thisKey < 11)
		{
			keyConsumed = true;
			enableSubmode(&subModeMidiFx[thisKey - 6]);
		}
	}

	return keyConsumed;
}

void OmxModeDrum::onKeyHeldUpdate(OMXKeypadEvent e)
{
	if (isSubmodeEnabled())
	{
		activeSubmode->onKeyHeldUpdate(e);
		return;
	}

	if (onKeyHeldSelMidiFX(e))
		return;
}

// TODO : Instantiate these outside of class so they are global
midimacro::MidiMacroInterface *OmxModeDrum::getActiveMacro()
{
	switch (midiMacroConfig.midiMacro)
	{
	case 1:
		return &m8Macro_;
	case 2:
		return &nornsMarco_;
	case 3:
		return &delugeMacro_;
	}
	return nullptr;
}

void OmxModeDrum::updateLEDs()
{
	if (isSubmodeEnabled())
	{
		if (activeSubmode->updateLEDs())
			return;
	}

    bool blinkState = omxLeds.getBlinkState();
    bool slowBlink = omxLeds.getSlowBlinkState();

	if (midiSettings.midiAUX)
	{
		// Blink left/right keys for octave select indicators.
		auto color1 = LIME;
		auto color2 = MAGENTA;

		for (int q = 1; q < LED_COUNT; q++)
		{
			if (midiSettings.midiKeyState[q] == -1)
			{
				if (colorConfig.midiBg_Hue == 0)
				{
					strip.setPixelColor(q, LEDOFF);
				}
				else if (colorConfig.midiBg_Hue == 32)
				{
					strip.setPixelColor(q, LOWWHITE);
				}
				else
				{
					strip.setPixelColor(q, strip.ColorHSV(colorConfig.midiBg_Hue, colorConfig.midiBg_Sat, colorConfig.midiBg_Brightness));
				}
			}
		}
		strip.setPixelColor(0, RED);
		strip.setPixelColor(1, color1);
		strip.setPixelColor(2, color2);

		strip.setPixelColor(3, BLUE); // Load
		strip.setPixelColor(4, ORANGE); // Save

		omxLeds.drawOctaveKeys(11, 12, midiSettings.octave);

        uint8_t mfxIndex = activeDrumKit.drumKeys[selDrumKey].midifx;

		// MidiFX off
		strip.setPixelColor(5, (mfxIndex >= NUM_MIDIFX_GROUPS ? colorConfig.selMidiFXGRPOffColor : colorConfig.midiFXGRPOffColor));

		for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
		{
			auto mfxColor = (i == mfxIndex) ? colorConfig.selMidiFXGRPColor : colorConfig.midiFXGRPColor;

			strip.setPixelColor(6 + i, mfxColor);
		}

		strip.setPixelColor(20, mfxQuickEdit_ && blinkState ? LEDOFF : colorConfig.mfxQuickEdit);
		strip.setPixelColor(22, colorConfig.gotoArpParams);
		strip.setPixelColor(23, colorConfig.nextArpPattern);

		if (mfxIndex < NUM_MIDIFX_GROUPS)
		{
			uint8_t octaveRange = subModeMidiFx[mfxIndex].getArpOctaveRange();
			if (octaveRange == 0)
			{
				strip.setPixelColor(24, colorConfig.nextArpOctave);
			}
			else
			{
				// Serial.println("Blink Octave: " + String(octaveRange));
				bool blinkOctave = omxLeds.getBlinkPattern(octaveRange);

				strip.setPixelColor(24, blinkOctave ? colorConfig.nextArpOctave : LEDOFF);
			}

			bool isOn = subModeMidiFx[mfxIndex].isArpOn() && blinkState;
			bool isHoldOn = subModeMidiFx[mfxIndex].isArpHoldOn();

			strip.setPixelColor(25, isHoldOn ? colorConfig.arpHoldOn : colorConfig.arpHoldOff);
			strip.setPixelColor(26, isOn ? colorConfig.arpOn : colorConfig.arpOff);
		}
		else
		{
			strip.setPixelColor(25, colorConfig.arpHoldOff);
			strip.setPixelColor(26, colorConfig.arpOff);
		}
	}
	else
	{
        for(uint8_t k = 1; k < 27; k++)
        {
            auto drumKey = activeDrumKit.drumKeys[k-1];

            bool drumKeyOn = midiSettings.midiKeyState[k] >= 0;

            if(k-1 == selDrumKey)
            {
                drumKeyOn = drumKeyOn || slowBlink;
            }

	        uint16_t hue = map(drumKey.hue, 0, 255, 0, 65535);

            strip.setPixelColor(k, strip.gamma32(strip.ColorHSV(hue, drumKeyOn ? 200 : 255, drumKeyOn ? 255 : 160)));
        }
	}

	// if (isSubmodeEnabled())
	// {
	// 	bool blinkStateSlow = omxLeds.getSlowBlinkState();

	// 	auto auxColor = (blinkStateSlow ? RED : LEDOFF);
	// 	strip.setPixelColor(0, auxColor);
	// }
}

void OmxModeDrum::onDisplayUpdate()
{
	if (isSubmodeEnabled())
	{
		if (omxLeds.isDirty())
		{
			updateLEDs();
		}
		activeSubmode->onDisplayUpdate();
		return;
	}

	bool macroConsumesDisplay = false;

	if (macroActive_ && activeMacro_ != nullptr)
	{
		activeMacro_->drawLEDs();
		macroConsumesDisplay = activeMacro_->consumesDisplay();
	}
	else
	{
		if (omxLeds.isDirty())
		{
			updateLEDs();
		}
	}

	if (macroConsumesDisplay)
	{
		activeMacro_->onDisplayUpdate();
	}
	else
	{
		if (omxDisp.isDirty())
		{ // DISPLAY
			if (!encoderConfig.enc_edit)
			{
				if (params.getSelPage() == DRUMPAGE_DRUMKEY)
				{
                    auto drumKey = activeDrumKit.drumKeys[selDrumKey];

					omxDisp.clearLegends();

					omxDisp.legends[0] = "NOTE";
					omxDisp.legends[1] = "CH";
					omxDisp.legends[2] = "VEL";
					omxDisp.legends[3] = "FX#";
					omxDisp.legendVals[0] = drumKey.noteNum;
					omxDisp.legendVals[1] = drumKey.chan;
					omxDisp.legendVals[2] = drumKey.vel;
                    if(drumKey.midifx >= NUM_MIDIFX_GROUPS)
                    {
                        omxDisp.legendText[3] = "OFF";
                    }
                    else
                    {
					    omxDisp.legendVals[3] = drumKey.midifx + 1;
                    }
				}
                else if (params.getSelPage() == DRUMPAGE_DRUMKEY2)
				{
                    auto drumKey = activeDrumKit.drumKeys[selDrumKey];

					omxDisp.clearLegends();

					omxDisp.legends[0] = "HUE";
					omxDisp.legends[1] = "HUE";
					omxDisp.legends[2] = "COPY";
					omxDisp.legends[3] = "PAST";
					omxDisp.legendVals[0] = drumKey.hue;
					omxDisp.legendText[1] = "RND";
					omxDisp.legendVals[2] = selDrumKey + 1;
					omxDisp.legendVals[3] = selDrumKey + 1;
				}
				else if (params.getSelPage() == DRUMPAGE_INSPECT)
				{
					omxDisp.clearLegends();

					omxDisp.legends[0] = "P CC";
					omxDisp.legends[1] = "P VAL";
					omxDisp.legends[2] = "NOTE";
					omxDisp.legends[3] = "VEL";
					omxDisp.legendVals[0] = potSettings.potCC;
					omxDisp.legendVals[1] = potSettings.potVal;
					omxDisp.legendVals[2] = midiSettings.midiLastNote;
					omxDisp.legendVals[3] = midiSettings.midiLastVel;
				}
				else if (params.getSelPage() == DRUMPAGE_POTSANDMACROS) // SUBMODE_MIDI3
				{
					omxDisp.clearLegends();

					omxDisp.legends[0] = "PBNK"; // Potentiometer Banks
					omxDisp.legends[1] = "THRU"; // MIDI thru (usb to hardware)
					omxDisp.legends[2] = "MCRO"; // Macro mode
					omxDisp.legends[3] = "M-CH";
					omxDisp.legendVals[0] = potSettings.potbank + 1;
					omxDisp.legendText[1] = midiSettings.midiSoftThru ? "On" : "Off";
					omxDisp.legendText[2] = macromodes[midiMacroConfig.midiMacro];
					omxDisp.legendVals[3] = midiMacroConfig.midiMacroChan;
				}
                else if (params.getSelPage() == DRUMPAGE_SCALES) // SCALES
				{
					omxDisp.clearLegends();
					omxDisp.legends[0] = "ROOT";
					omxDisp.legends[1] = "SCALE";
					omxDisp.legends[2] = "LOCK";
					omxDisp.legends[3] = "GROUP";
					omxDisp.legendVals[0] = -127;
					if (scaleConfig.scalePattern < 0)
					{
						omxDisp.legendVals[1] = -127;
						omxDisp.legendText[1] = "Off";
					}
					else
					{
						omxDisp.legendVals[1] = scaleConfig.scalePattern;
					}

					omxDisp.legendVals[2] = -127;
					omxDisp.legendVals[3] = -127;

					omxDisp.legendText[0] = musicScale->getNoteName(scaleConfig.scaleRoot);
					omxDisp.legendText[2] = scaleConfig.lockScale ? "On" : "Off";
					omxDisp.legendText[3] = scaleConfig.group16 ? "On" : "Off";
				}
				else if (params.getSelPage() == DRUMPAGE_CFG) // CONFIG
				{
					omxDisp.clearLegends();
					omxDisp.setLegend(0,"P CC", "CFG");
					omxDisp.setLegend(1,"CLR", "STOR");
					omxDisp.setLegend(2,"QUANT", "1/" + String(kArpRates[clockConfig.globalQuantizeStepIndex]));
					omxDisp.setLegend(3,"CV M", cvNoteUtil.getTriggerModeDispName());
				}

				omxDisp.dispGenericMode2(params.getNumPages(), params.getSelPage(), params.getSelParam(), getEncoderSelect());
			}
		}
	}
}

// void onDisplayUpdateLoadKit()
// {

// }

// incoming midi note on
void OmxModeDrum::inMidiNoteOn(byte channel, byte note, byte velocity)
{
	// midiSettings.midiLastNote = note;
	// midiSettings.midiLastVel = velocity;
	// int whatoct = (note / 12);
	// int thisKey;
	// uint32_t keyColor = MIDINOTEON;

	// if ((whatoct % 2) == 0)
	// {
	// 	thisKey = note - (12 * whatoct);
	// }
	// else
	// {
	// 	thisKey = note - (12 * whatoct) + 12;
	// }
	// if (whatoct == 0)
	// { // ORANGE,YELLOW,GREEN,MAGENTA,CYAN,BLUE,LIME,LTPURPLE
	// }
	// else if (whatoct == 1)
	// {
	// 	keyColor = ORANGE;
	// }
	// else if (whatoct == 2)
	// {
	// 	keyColor = YELLOW;
	// }
	// else if (whatoct == 3)
	// {
	// 	keyColor = GREEN;
	// }
	// else if (whatoct == 4)
	// {
	// 	keyColor = MAGENTA;
	// }
	// else if (whatoct == 5)
	// {
	// 	keyColor = CYAN;
	// }
	// else if (whatoct == 6)
	// {
	// 	keyColor = LIME;
	// }
	// else if (whatoct == 7)
	// {
	// 	keyColor = CYAN;
	// }
	// strip.setPixelColor(midiKeyMap[thisKey], keyColor); //  Set pixel's color (in RAM)
	// 													//	dirtyPixels = true;
	// strip.show();
	// omxDisp.setDirty();
}

void OmxModeDrum::inMidiNoteOff(byte channel, byte note, byte velocity)
{
	// int whatoct = (note / 12);
	// int thisKey;
	// if ((whatoct % 2) == 0)
	// {
	// 	thisKey = note - (12 * whatoct);
	// }
	// else
	// {
	// 	thisKey = note - (12 * whatoct) + 12;
	// }
	// strip.setPixelColor(midiKeyMap[thisKey], LEDOFF); //  Set pixel's color (in RAM)
	// 												  //	dirtyPixels = true;
	// strip.show();
	// omxDisp.setDirty();
}

void OmxModeDrum::inMidiControlChange(byte channel, byte control, byte value)
{
	auto activeMacro = getActiveMacro();

	if (activeMacro != nullptr)
	{
		activeMacro->inMidiControlChange(channel, control, value);
	}
}

void OmxModeDrum::SetScale(MusicScales *scale)
{
	this->musicScale = scale;
	m8Macro_.setScale(scale);
	nornsMarco_.setScale(scale);
}

void OmxModeDrum::randomizeHues()
{
    for(uint8_t i = 0; i < 26; i++)
    {
        activeDrumKit.drumKeys[i].hue = random(255);
    }
}

void OmxModeDrum::enableSubmode(SubmodeInterface *subMode)
{
	if (activeSubmode != nullptr)
	{
		activeSubmode->setEnabled(false);
	}

	activeSubmode = subMode;
	activeSubmode->setEnabled(true);
	omxDisp.setDirty();
}

void OmxModeDrum::disableSubmode()
{
	if (activeSubmode != nullptr)
	{
		activeSubmode->setEnabled(false);
	}

	midiSettings.midiAUX = false;
	mfxQuickEdit_ = false;
	activeSubmode = nullptr;
	omxDisp.setDirty();
}

bool OmxModeDrum::isSubmodeEnabled()
{
	if (activeSubmode == nullptr)
		return false;

	if (activeSubmode->isEnabled() == false)
	{
		disableSubmode();
		midiSettings.midiAUX = false;
		return false;
	}

	return true;
}

void OmxModeDrum::drumKeyDown(uint8_t keyIndex)
{
    auto drumKey = activeDrumKit.drumKeys[keyIndex - 1];

    MidiNoteGroup noteGroup = omxUtil.midiDrumNoteOn(keyIndex, drumKey.noteNum, drumKey.vel, drumKey.chan);

	if (noteGroup.noteNumber == 255)
		return;

    selDrumKey = keyIndex - 1;

	noteGroup.unknownLength = true;
	noteGroup.prevNoteNumber = noteGroup.noteNumber;

	if (drumKey.midifx < NUM_MIDIFX_GROUPS)
	{
		subModeMidiFx[drumKey.midifx].noteInput(noteGroup);
	}
	else
	{
		onNotePostFX(noteGroup);
	}
}

void OmxModeDrum::drumKeyUp(uint8_t keyIndex)
{
    MidiNoteGroup noteGroup = omxUtil.midiDrumNoteOff(keyIndex);

	if (noteGroup.noteNumber == 255)
		return;

    auto drumKey = activeDrumKit.drumKeys[keyIndex - 1];

	noteGroup.unknownLength = true;
	noteGroup.prevNoteNumber = noteGroup.noteNumber;

	if (drumKey.midifx < NUM_MIDIFX_GROUPS)
	{
		subModeMidiFx[drumKey.midifx].noteInput(noteGroup);
	}
	else
	{
		onNotePostFX(noteGroup);
	}
}

// Called via doNoteOnForwarder
void OmxModeDrum::doNoteOn(uint8_t keyIndex)
{
	MidiNoteGroup noteGroup = omxUtil.midiNoteOn2(musicScale, keyIndex, midiSettings.defaultVelocity, sysSettings.midiChannel);

	if (noteGroup.noteNumber == 255)
		return;

	// Serial.println("doNoteOn: " + String(noteGroup.noteNumber));

	noteGroup.unknownLength = true;
	noteGroup.prevNoteNumber = noteGroup.noteNumber;

    onNotePostFX(noteGroup);

	// if (mfxIndex_ < NUM_MIDIFX_GROUPS)
	// {
	// 	subModeMidiFx[mfxIndex_].noteInput(noteGroup);
	// 	// subModeMidiFx.noteInput(noteGroup);
	// }
	// else
	// {
	// 	onNotePostFX(noteGroup);
	// }
}

// Called via doNoteOnForwarder
void OmxModeDrum::doNoteOff(uint8_t keyIndex)
{
	MidiNoteGroup noteGroup = omxUtil.midiNoteOff2(keyIndex, sysSettings.midiChannel);

	if (noteGroup.noteNumber == 255)
		return;

	// Serial.println("doNoteOff: " + String(noteGroup.noteNumber));

	noteGroup.unknownLength = true;
	noteGroup.prevNoteNumber = noteGroup.noteNumber;

    onNotePostFX(noteGroup);

	// if (mfxIndex_ < NUM_MIDIFX_GROUPS)
	// {
	// 	subModeMidiFx[mfxIndex_].noteInput(noteGroup);
	// 	// subModeMidiFx.noteInput(noteGroup);
	// }
	// else
	// {
	// 	onNotePostFX(noteGroup);
	// }
}

// Called by the midiFX group when a note exits it's FX Pedalboard
void OmxModeDrum::onNotePostFX(MidiNoteGroup note)
{
	if (note.noteOff)
	{
		// Serial.println("OmxModeDrum::onNotePostFX noteOff: " + String(note.noteNumber));

		if (note.sendMidi)
		{
			MM::sendNoteOff(note.noteNumber, note.velocity, note.channel);
		}
		if (note.sendCV)
		{
			cvNoteUtil.cvNoteOff(note.noteNumber);
		}
	}
	else
	{
		if (note.unknownLength == false)
		{
			uint32_t noteOnMicros = note.noteonMicros; // TODO Might need to be set to current micros
			pendingNoteOns.insert(note.noteNumber, note.velocity, note.channel, noteOnMicros, note.sendCV);

			// Serial.println("StepLength: " + String(note.stepLength));

			uint32_t noteOffMicros = noteOnMicros + (note.stepLength * clockConfig.step_micros);
			pendingNoteOffs.insert(note.noteNumber, note.channel, noteOffMicros, note.sendCV);

			// Serial.println("noteOnMicros: " + String(noteOnMicros));
			// Serial.println("noteOffMicros: " + String(noteOffMicros));
		}
		else
		{
			// Serial.println("OmxModeDrum::onNotePostFX noteOn: " + String(note.noteNumber));

			if (note.sendMidi)
			{
				midiSettings.midiLastNote = note.noteNumber;
				midiSettings.midiLastVel = note.velocity;
				MM::sendNoteOn(note.noteNumber, note.velocity, note.channel);
			}
			if (note.sendCV)
			{
				cvNoteUtil.cvNoteOn(note.noteNumber);
			}
		}
	}

	// uint32_t noteOnMicros = note.noteonMicros; // TODO Might need to be set to current micros
	// pendingNoteOns.insert(note.noteNumber, note.velocity, note.channel, noteOnMicros, note.sendCV);

	// uint32_t noteOffMicros = noteOnMicros + (note.stepLength * clockConfig.step_micros);
	// pendingNoteOffs.insert(note.noteNumber, note.channel, noteOffMicros, note.sendCV);
}

void OmxModeDrum::onPendingNoteOff(int note, int channel)
{
	// Serial.println("OmxModeEuclidean::onPendingNoteOff " + String(note) + " " + String(channel));
	// subModeMidiFx.onPendingNoteOff(note, channel);

	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		subModeMidiFx[i].onPendingNoteOff(note, channel);
	}
}

int OmxModeDrum::saveToDisk(int startingAddress, Storage *storage)
{
	int saveSize = sizeof(DrumKit);

	for (uint8_t saveIndex = 0; saveIndex < NUM_DRUM_KITS; saveIndex++)
	{
        auto saveBytesPtr = (byte *)(&drumKits[saveIndex]);
        for (int j = 0; j < saveSize; j++)
        {
            storage->write(startingAddress + j, *saveBytesPtr++);
        }

        startingAddress += saveSize;
	}

	return startingAddress;
}

int OmxModeDrum::loadFromDisk(int startingAddress, Storage *storage)
{
	int saveSize = sizeof(DrumKit); // 5 * 26 = 130

	// int drumKeySize = sizeof(DrumKeySettings);

	// Serial.println((String)"DrumKit Size: " + saveSize + " drumKeySize: " + drumKeySize); // 5 - 130 - 1040 bytes

    for (uint8_t saveIndex = 0; saveIndex < NUM_DRUM_KITS; saveIndex++)
    {
        // auto drumKit = DrumKit{};
        // auto current = (byte *)&drumKit;
        // for (int j = 0; j < saveSize; j++)
        // {
        //     *current = storage->read(startingAddress + j);
        //     current++;
        // }

        // drumKits[saveIndex].CopyFrom(drumKit);

		// Write bytes to heap
		auto current = (byte *)&drumKits[saveIndex];
		for (int j = 0; j < saveSize; j++)
        {
            *current = storage->read(startingAddress + j);
            current++;
        }

        startingAddress += saveSize;
    }

    loadKit(0);

	return startingAddress;
}
