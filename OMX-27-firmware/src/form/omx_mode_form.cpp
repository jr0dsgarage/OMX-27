#include "omx_mode_form.h"
#include "../config.h"
#include "../consts/colors.h"
#include "../utils/omx_util.h"
#include "../utils/cvNote_util.h"
#include "../hardware/omx_disp.h"
#include "../hardware/omx_leds.h"
#include "../midi/midi.h"
#include "../utils/music_scales.h"
#include "../midi/noteoffs.h"

enum FormModePage {
    FORMPAGE_DRUMKEY, // Note, Chan, Vel, MidiFX
    FORMPAGE_DRUMKEY2, // Hue, RND Hue, Copy, Paste
    FORMPAGE_SCALES, // Hue,
    FORMPAGE_INSPECT, // Sent Pot CC, Last Note, Last Vel, Last Chan, Not editable, just FYI
    FORMPAGE_POTSANDMACROS, // PotBank, Thru, Macro, Macro Channel
    FORMPAGE_CFG,
    FORMPAGE_NUMPAGES
};

OmxModeForm::OmxModeForm()
{
    params.addPages(FORMPAGE_NUMPAGES);

    auxMacroManager_.setContext(this);
    auxMacroManager_.setMacroNoteOn(&OmxModeForm::doNoteOnForwarder);
    auxMacroManager_.setMacroNoteOff(&OmxModeForm::doNoteOffForwarder);
    auxMacroManager_.setSelectMidiFXFPTR(&OmxModeForm::selectMidiFXForwarder);

    presetManager.setContextPtr(this);
    presetManager.setDoSaveFunc(&OmxModeForm::doSaveKitForwarder);
    presetManager.setDoLoadFunc(&OmxModeForm::doLoadKitForwarder);
}

void OmxModeForm::InitSetup()
{
	initSetup = true;
}

void OmxModeForm::onModeActivated()
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
		subModeMidiFx[i].setNoteOutputFunc(&OmxModeForm::onNotePostFXForwarder, this);
	}

	pendingNoteOffs.setNoteOffFunction(&OmxModeForm::onPendingNoteOffForwarder, this);

	params.setSelPageAndParam(0, 0);
	encoderSelect = true;

    auxMacroManager_.onModeActivated();

    // activeDrumKit.CopyFrom(drumKits[selDrumKit]);

	// selectMidiFx(mfxIndex_, false);
}

void OmxModeForm::onModeDeactivated()
{
	stopSequencers();

	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		subModeMidiFx[i].setEnabled(false);
		subModeMidiFx[i].onModeChanged();
	}

    auxMacroManager_.onModeDectivated();
}

void OmxModeForm::stopSequencers()
{
	omxUtil.stopClocks();
	pendingNoteOffs.allOff();
}

void OmxModeForm::selectMidiFx(uint8_t mfxIndex, bool dispMsg)
{
    // uint8_t prevMidiFX = activeDrumKit.drumKeys[selDrumKey].midifx;
    
    // if(mfxIndex != prevMidiFX && prevMidiFX < NUM_MIDIFX_GROUPS)
    // {
    //     drumKeyUp(selDrumKey + 1);
    // }

    // activeDrumKit.drumKeys[selDrumKey].midifx = mfxIndex;

	// if(mfxQuickEdit_)
	// {
	// 	// Change the MidiFX Group being edited
	// 	if(mfxIndex < NUM_MIDIFX_GROUPS && mfxIndex != quickEditMfxIndex_)
	// 	{
	// 		enableSubmode(&subModeMidiFx[mfxIndex]);
	// 		subModeMidiFx[mfxIndex].enablePassthrough();
	// 		quickEditMfxIndex_ = mfxIndex;
	// 		dispMsg = false;
	// 	}
	// 	else if(mfxIndex >= NUM_MIDIFX_GROUPS)
	// 	{
	// 		disableSubmode();
	// 	}
	// }

	// for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	// {
	// 	subModeMidiFx[i].setSelected(i == mfxIndex);
	// }

	// if (dispMsg)
	// {
	// 	if (mfxIndex < NUM_MIDIFX_GROUPS)
	// 	{
	// 		omxDisp.displayMessageTimed("MidiFX " + String(mfxIndex + 1), 5);
	// 	}
	// 	else
	// 	{
	// 		omxDisp.displayMessageTimed("MidiFX Off", 5);
	// 	}
	// }
}

void OmxModeForm::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
{
    if(auxMacroManager_.onPotChanged(potIndex, prevValue, newValue, analogDelta))
        return;

    omxUtil.sendPots(potIndex, sysSettings.midiChannel);
	omxDisp.setDirty();
}

void OmxModeForm::onClockTick()
{
	for (uint8_t i = 0; i < 5; i++)
	{
		// Lets them do things in background
		subModeMidiFx[i].onClockTick();
	}
}

void OmxModeForm::loopUpdate(Micros elapsedTime)
{
	for (uint8_t i = 0; i < 5; i++)
	{
		// Lets them do things in background
		subModeMidiFx[i].loopUpdate();
	}

	// Can be modified by scale MidiFX
	musicScale->calculateScaleIfModified(scaleConfig.scaleRoot, scaleConfig.scalePattern);
	
}

bool OmxModeForm::getEncoderSelect()
{
	// return encoderSelect && !midiSettings.midiAUX && !isDrumKeyHeld();

	return encoderSelect && !midiSettings.midiAUX;

}

void OmxModeForm::onEncoderChanged(Encoder::Update enc)
{
    if (auxMacroManager_.onEncoderChanged(enc))
        return;

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
    
    if (selPage == FORMPAGE_POTSANDMACROS)
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
    else if (selPage == FORMPAGE_SCALES)
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
	else if(selPage == FORMPAGE_CFG)
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

void OmxModeForm::onEncoderButtonDown()
{
    if(auxMacroManager_.onEncoderButtonDown())
        return;

	if (params.isPageAndParam(FORMPAGE_CFG, 0))
	{
		auxMacroManager_.enableSubmode(&omxUtil.subModePotConfig);
		omxDisp.isDirty();
		return;
	}

	encoderSelect = !encoderSelect;
	omxDisp.isDirty();
}

void OmxModeForm::onEncoderButtonUp()
{
}

void OmxModeForm::onEncoderButtonDownLong()
{
}

bool OmxModeForm::shouldBlockEncEdit()
{
    if (auxMacroManager_.shouldBlockEncEdit())
        return true;

    return false;
}

void OmxModeForm::saveKit(uint8_t saveIndex)
{
    // drumKits[saveIndex].CopyFrom(activeDrumKit);
    // selDrumKit = saveIndex;
}

void OmxModeForm::loadKit(uint8_t loadIndex)
{
    // activeDrumKit.CopyFrom(drumKits[loadIndex]);
    // selDrumKit = loadIndex;
}

void OmxModeForm::onKeyUpdate(OMXKeypadEvent e)
{
    if (auxMacroManager_.onKeyUpdate(e))
        return; // Key consumed by macro

    if (onKeyUpdateSelMidiFX(e))
        return;

    int thisKey = e.key();

    // AUX KEY
    if (thisKey == 0)
    {
        midiSettings.midiAUX = e.down();
    }
    // REGULAR KEY PRESSES
    else
    {
        // IGNORE LONG PRESS EVENTS
        if (!e.held())
        { 
            if (e.down())
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
                    if (auxMacroManager_.isMFXQuickEditEnabled() == false && (thisKey == 1 || thisKey == 2)) // Change Param selection
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
                    else if (thisKey == 3)
                    {
                        // changeMode(DRUMMODE_LOADKIT);
                        return;
                    }
                    else if (thisKey == 4)
                    {
                        // changeMode(DRUMMODE_SAVEKIT);
                        return;
                    }
                    else if (thisKey == 11 || thisKey == 12)
                    {
                        // saveKit(selDrumKit);

                        // int8_t amt = thisKey == 11 ? -1 : 1;
                        // uint8_t newKitIndex = (selDrumKit + NUM_DRUM_KITS + amt) % NUM_DRUM_KITS;
                        // loadKit(newKitIndex);

                        // omxDisp.displayMessage("Loaded " + String(newKitIndex + 1));
                    }
                }

                if (!keyConsumed)
                {
                    // drumKeyDown(thisKey);
                }
            }
            else if (!e.down())
            {
                // drumKeyUp(thisKey);
            }
        }
    }

    omxLeds.setDirty();
    omxDisp.setDirty();
}

bool OmxModeForm::onKeyUpdateSelMidiFX(OMXKeypadEvent e)
{
    uint8_t mfxIndex = 0;

    if (auxMacroManager_.onKeyUpdateAuxMFXShortcuts(e, mfxIndex))
        return true;

    return false;
}

bool OmxModeForm::onKeyHeldSelMidiFX(OMXKeypadEvent e)
{
    uint8_t mfxIndex = 0;

	if (auxMacroManager_.onKeyHeldAuxMFXShortcuts(e, mfxIndex))
        return true;

    return false;
}

void OmxModeForm::onKeyHeldUpdate(OMXKeypadEvent e)
{
	if(auxMacroManager_.onKeyHeldUpdate(e))
        return;

	if (onKeyHeldSelMidiFX(e))
		return;
}

void OmxModeForm::updateLEDs()
{
    omxLeds.setAllLEDS(0, 0, 0);

    // bool blinkState = omxLeds.getBlinkState();
    // bool slowBlink = omxLeds.getSlowBlinkState();

	if (midiSettings.midiAUX)
	{
        uint8_t selMFXIndex = 0;
        auxMacroManager_.UpdateAUXLEDS(selMFXIndex);
	}
	else
	{
        // for(uint8_t k = 1; k < 27; k++)
        // {
        //     auto drumKey = activeDrumKit.drumKeys[k-1];

        //     bool drumKeyOn = midiSettings.midiKeyState[k] >= 0;

        //     if(k-1 == selDrumKey)
        //     {
        //         drumKeyOn = drumKeyOn || slowBlink;
        //     }

	    //     uint16_t hue = map(drumKey.hue, 0, 255, 0, 65535);

        //     strip.setPixelColor(k, strip.gamma32(strip.ColorHSV(hue, drumKeyOn ? 200 : 255, drumKeyOn ? 255 : 160)));
        // }
	}

	// if (isSubmodeEnabled())
	// {
	// 	bool blinkStateSlow = omxLeds.getSlowBlinkState();

	// 	auto auxColor = (blinkStateSlow ? RED : LEDOFF);
	// 	strip.setPixelColor(0, auxColor);
	// }
}

void OmxModeForm::onDisplayUpdate()
{
    if (auxMacroManager_.updateLEDs() == false && omxLeds.isDirty())
    {
        // Macro or submode is off, update our LEDs
        updateLEDs();
    }

    // If true, macro or submode is on and consuming display
    if (auxMacroManager_.onDisplayUpdate())
        return;

    // If this is true we are in mode selection menu
    if (encoderConfig.enc_edit)
        return;

    if (omxDisp.isDirty())
    {
        if (params.getSelPage() == FORMPAGE_INSPECT)
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
        else if (params.getSelPage() == FORMPAGE_POTSANDMACROS) // SUBMODE_MIDI3
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
        else if (params.getSelPage() == FORMPAGE_SCALES) // SCALES
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
        else if (params.getSelPage() == FORMPAGE_CFG) // CONFIG
        {
            omxDisp.clearLegends();
            omxDisp.setLegend(0, "P CC", "CFG");
            omxDisp.setLegend(1, "CLR", "STOR");
            omxDisp.setLegend(2, "QUANT", "1/" + String(kArpRates[clockConfig.globalQuantizeStepIndex]));
            omxDisp.setLegend(3, "CV M", cvNoteUtil.getTriggerModeDispName());
        }

        omxDisp.dispGenericMode2(params.getNumPages(), params.getSelPage(), params.getSelParam(), getEncoderSelect());
    }
}

// void onDisplayUpdateLoadKit()
// {

// }

// incoming midi note on
void OmxModeForm::inMidiNoteOn(byte channel, byte note, byte velocity)
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

void OmxModeForm::inMidiNoteOff(byte channel, byte note, byte velocity)
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

void OmxModeForm::inMidiControlChange(byte channel, byte control, byte value)
{
    if(auxMacroManager_.inMidiControlChange(channel, control, value))
        return;
}

void OmxModeForm::SetScale(MusicScales *scale)
{
	musicScale = scale;
    auxMacroManager_.SetScale(scale);
}

// void OmxModeForm::drumKeyDown(uint8_t keyIndex)
// {
//     auto drumKey = activeDrumKit.drumKeys[keyIndex - 1];

//     MidiNoteGroup noteGroup = omxUtil.midiDrumNoteOn(keyIndex, drumKey.noteNum, drumKey.vel, drumKey.chan);

// 	if (noteGroup.noteNumber == 255)
// 		return;

//     selDrumKey = keyIndex - 1;

// 	noteGroup.unknownLength = true;
// 	noteGroup.prevNoteNumber = noteGroup.noteNumber;

// 	if (drumKey.midifx < NUM_MIDIFX_GROUPS)
// 	{
// 		subModeMidiFx[drumKey.midifx].noteInput(noteGroup);
// 	}
// 	else
// 	{
// 		onNotePostFX(noteGroup);
// 	}
// }

// void OmxModeForm::drumKeyUp(uint8_t keyIndex)
// {
//     MidiNoteGroup noteGroup = omxUtil.midiDrumNoteOff(keyIndex);

// 	if (noteGroup.noteNumber == 255)
// 		return;

//     auto drumKey = activeDrumKit.drumKeys[keyIndex - 1];

// 	noteGroup.unknownLength = true;
// 	noteGroup.prevNoteNumber = noteGroup.noteNumber;

// 	if (drumKey.midifx < NUM_MIDIFX_GROUPS)
// 	{
// 		subModeMidiFx[drumKey.midifx].noteInput(noteGroup);
// 	}
// 	else
// 	{
// 		onNotePostFX(noteGroup);
// 	}
// }

// Called via doNoteOnForwarder
void OmxModeForm::doNoteOn(uint8_t keyIndex)
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
void OmxModeForm::doNoteOff(uint8_t keyIndex)
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
void OmxModeForm::onNotePostFX(MidiNoteGroup note)
{
	if (note.noteOff)
	{
		// Serial.println("OmxModeForm::onNotePostFX noteOff: " + String(note.noteNumber));

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
			// Serial.println("OmxModeForm::onNotePostFX noteOn: " + String(note.noteNumber));

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

void OmxModeForm::onPendingNoteOff(int note, int channel)
{
	// Serial.println("OmxModeEuclidean::onPendingNoteOff " + String(note) + " " + String(channel));
	// subModeMidiFx.onPendingNoteOff(note, channel);

	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		subModeMidiFx[i].onPendingNoteOff(note, channel);
	}
}

int OmxModeForm::saveToDisk(int startingAddress, Storage *storage)
{
	// int saveSize = sizeof(DrumKit);

	// for (uint8_t saveIndex = 0; saveIndex < NUM_DRUM_KITS; saveIndex++)
	// {
    //     auto saveBytesPtr = (byte *)(&drumKits[saveIndex]);
    //     for (int j = 0; j < saveSize; j++)
    //     {
    //         storage->write(startingAddress + j, *saveBytesPtr++);
    //     }

    //     startingAddress += saveSize;
	// }

	return startingAddress;
}

int OmxModeForm::loadFromDisk(int startingAddress, Storage *storage)
{
	// int saveSize = sizeof(DrumKit); // 5 * 26 = 130

	// // int drumKeySize = sizeof(DrumKeySettings);

	// // Serial.println((String)"DrumKit Size: " + saveSize + " drumKeySize: " + drumKeySize); // 5 - 130 - 1040 bytes

    // for (uint8_t saveIndex = 0; saveIndex < NUM_DRUM_KITS; saveIndex++)
    // {
    //     // auto drumKit = DrumKit{};
    //     // auto current = (byte *)&drumKit;
    //     // for (int j = 0; j < saveSize; j++)
    //     // {
    //     //     *current = storage->read(startingAddress + j);
    //     //     current++;
    //     // }

    //     // drumKits[saveIndex].CopyFrom(drumKit);

	// 	// Write bytes to heap
	// 	auto current = (byte *)&drumKits[saveIndex];
	// 	for (int j = 0; j < saveSize; j++)
    //     {
    //         *current = storage->read(startingAddress + j);
    //         current++;
    //     }

    //     startingAddress += saveSize;
    // }

    // loadKit(0);

	return startingAddress;
}
