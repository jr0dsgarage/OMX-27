#include "omx_mode_midi_keyboard.h"
#include "config.h"
#include "colors.h"
#include "omx_util.h"
#include "omx_disp.h"
#include "omx_leds.h"
#include "MM.h"
#include "music_scales.h"
#include "noteoffs.h"
#include "sequencer.h"

const int kSelMidiFXOffColor = SALMON;
const int kMidiFXOffColor = RED;

const int kSelMidiFXColor = LTCYAN;
const int kMidiFXColor = BLUE;

OmxModeMidiKeyboard::OmxModeMidiKeyboard()
{
    // Add 4 pages
    params.addPage(4);
    params.addPage(4);
    params.addPage(4);
    params.addPage(4);
    params.addPage(4);

    // subModeMidiFx.setNoteOutputFunc(&OmxModeMidiKeyboard::onNotePostFXForwarder, this);

    m8Macro_.setDoNoteOn(&OmxModeMidiKeyboard::doNoteOnForwarder, this);
    m8Macro_.setDoNoteOff(&OmxModeMidiKeyboard::doNoteOffForwarder, this);
    nornsMarco_.setDoNoteOn(&OmxModeMidiKeyboard::doNoteOnForwarder, this);
    nornsMarco_.setDoNoteOff(&OmxModeMidiKeyboard::doNoteOffForwarder, this);
}

void OmxModeMidiKeyboard::InitSetup()
{
    initSetup = true;
}

void OmxModeMidiKeyboard::onModeActivated()
{
    // auto init when activated
    if (!initSetup)
    {
        InitSetup();
    }

    sequencer.playing = false;
    stopSequencers();

    omxLeds.setDirty();
    omxDisp.setDirty();

    for(uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
    {
        subModeMidiFx[i].setNoteOutputFunc(&OmxModeMidiKeyboard::onNotePostFXForwarder, this);
    }

    pendingNoteOffs.setNoteOffFunction(&OmxModeMidiKeyboard::onPendingNoteOffForwarder, this);

    params.setSelPageAndParam(0, 0);
    encoderSelect = true;
}

void OmxModeMidiKeyboard::stopSequencers()
{
	MM::stopClock();
    pendingNoteOffs.allOff();
}

// void OmxModeMidiKeyboard::changePage(int amt)
// {
//     midiPageParams.mmpage = constrain(midiPageParams.mmpage + amt, 0, midiPageParams.numPages - 1);
//     midiPageParams.miparam = midiPageParams.mmpage * NUM_DISP_PARAMS;
// }

// void OmxModeMidiKeyboard::setParam(int paramIndex)
// {
//     if (paramIndex >= 0)
//     {
//         midiPageParams.miparam = paramIndex % midiPageParams.numParams;
//     }
//     else
//     {
//         midiPageParams.miparam = (paramIndex + midiPageParams.numParams) % midiPageParams.numParams;
//     }

//     // midiPageParams.miparam  = (midiPageParams.miparam  + 1) % 15;
//     midiPageParams.mmpage = midiPageParams.miparam / NUM_DISP_PARAMS;
// }

void OmxModeMidiKeyboard::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
{
     if (isSubmodeEnabled() && activeSubmode->usesPots())
    {
        activeSubmode->onPotChanged(potIndex, prevValue, newValue, analogDelta);
        return;
    }

    auto activeMacro = getActiveMacro();

    bool macroConsumesPots = false;
    if(activeMacro != nullptr)
    {
        macroConsumesPots = activeMacro->consumesPots();
    }

    // Note, these get sent even if macro mode is not active
    if(macroConsumesPots)
    {
        activeMacro->onPotChanged(potIndex, prevValue, newValue, analogDelta);
    }
    else
    {
        omxUtil.sendPots(potIndex, sysSettings.midiChannel);
    }

    // if (midiMacroConfig.midiMacro)
    // {
    //     omxUtil.sendPots(potIndex, midiMacroConfig.midiMacroChan);
    // }
    // else
    // {
    // }

    omxDisp.setDirty();
}

void OmxModeMidiKeyboard::loopUpdate()
{
    if (isSubmodeEnabled())
    {
        activeSubmode->loopUpdate();
    }
}


// Handles selecting params using encoder
void OmxModeMidiKeyboard::onEncoderChangedSelectParam(Encoder::Update enc)
{
    if(enc.dir() == 0) return;

    if (enc.dir() < 0) // if turn CCW
    {
        params.decrementParam();
    }
    else if (enc.dir() > 0) // if turn CW
    {
        params.incrementParam();
    }

    omxDisp.setDirty();
}

void OmxModeMidiKeyboard::onEncoderChanged(Encoder::Update enc)
{
    if (isSubmodeEnabled())
    {
        activeSubmode->onEncoderChanged(enc);
        return;
    }

    bool macroConsumesDisplay = false;

    if(macroActive_ && activeMacro_ != nullptr)
    {
        macroConsumesDisplay = activeMacro_->consumesDisplay();
    }

    if(macroConsumesDisplay)
    {
        activeMacro_->onEncoderChanged(enc);
        return;
    }

    if (encoderSelect)
    {
        onEncoderChangedSelectParam(enc);
        return;
    }

    if (organelleMotherMode)
    {
        // CHANGE PAGE
        if (params.getSelParam() == 0)
        {
            if (enc.dir() < 0)
            { // if turn ccw
                MM::sendControlChange(CC_OM2, 0, sysSettings.midiChannel);
            }
            else if (enc.dir() > 0)
            { // if turn cw
                MM::sendControlChange(CC_OM2, 127, sysSettings.midiChannel);
            }
        }

        omxDisp.setDirty();
    }

    if (midiSettings.midiAUX)
    {
        // if (enc.dir() < 0)
        // { // if turn ccw
        //     setParam(midiPageParams.miparam - 1);
        //     omxDisp.setDirty();
        // }
        // else if (enc.dir() > 0)
        // { // if turn cw
        //     setParam(midiPageParams.miparam + 1);
        //     omxDisp.setDirty();
        // }

        // change MIDI Background Color
        // midiBg_Hue = constrain(midiBg_Hue + (amt * 32), 0, 65534); // 65535
        return; // break;
    }

    auto amt = enc.accel(5); // where 5 is the acceleration factor if you want it, 0 if you don't)

    int8_t selPage = params.getSelPage() + 1; // Add one for readability
    int8_t selParam = params.getSelParam() + 1;

    // PAGE ONE
    if (selPage == 1)
    {
        if (selParam == 1)
        {
            // set octave
            midiSettings.newoctave = constrain(midiSettings.octave + amt, -5, 4);
            if (midiSettings.newoctave != midiSettings.octave)
            {
                midiSettings.octave = midiSettings.newoctave;
            }
        }
        else if (selParam == 2)
        {
            int newchan = constrain(sysSettings.midiChannel + amt, 1, 16);
            if (newchan != sysSettings.midiChannel)
            {
                sysSettings.midiChannel = newchan;
            }
        }
    }
    // PAGE TWO
    else if (selPage == 2)
    {
        if (selParam == 1)
        {
            int newrrchan = constrain(midiSettings.midiRRChannelCount + amt, 1, 16);
            if (newrrchan != midiSettings.midiRRChannelCount)
            {
                midiSettings.midiRRChannelCount = newrrchan;
                if (midiSettings.midiRRChannelCount == 1)
                {
                    midiSettings.midiRoundRobin = false;
                }
                else
                {
                    midiSettings.midiRoundRobin = true;
                }
            }
        }
        else if (selParam == 2)
        {
            midiSettings.midiRRChannelOffset = constrain(midiSettings.midiRRChannelOffset + amt, 0, 15);
        }
        else if (selParam == 3)
        {
            midiSettings.currpgm = constrain(midiSettings.currpgm + amt, 0, 127);

            if (midiSettings.midiRoundRobin)
            {
                for (int q = midiSettings.midiRRChannelOffset + 1; q < midiSettings.midiRRChannelOffset + midiSettings.midiRRChannelCount + 1; q++)
                {
                    MM::sendProgramChange(midiSettings.currpgm, q);
                }
            }
            else
            {
                MM::sendProgramChange(midiSettings.currpgm, sysSettings.midiChannel);
            }
        }
        else if (selParam == 4)
        {
            midiSettings.currbank = constrain(midiSettings.currbank + amt, 0, 127);
            // Bank Select is 2 mesages
            MM::sendControlChange(0, 0, sysSettings.midiChannel);
            MM::sendControlChange(32, midiSettings.currbank, sysSettings.midiChannel);
            MM::sendProgramChange(midiSettings.currpgm, sysSettings.midiChannel);
        }
    }
    // PAGE THREE
    else if (selPage == 3)
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
    // PAGE FOUR - SCALES
    else if (selPage == 4)
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

    omxDisp.setDirty();
}

void OmxModeMidiKeyboard::onEncoderButtonDown()
{
    if (isSubmodeEnabled())
    {
        activeSubmode->onEncoderButtonDown();
        return;
    }

    bool macroConsumesDisplay = false;
    if(macroActive_ && activeMacro_ != nullptr)
    {
        macroConsumesDisplay = activeMacro_->consumesDisplay();
    }

    if(macroConsumesDisplay)
    {
        activeMacro_->onEncoderButtonDown();
        return;
    }

    if(params.getSelPage() == 4 && params.getSelParam() == 0)
    {
        enableSubmode(&subModePotConfig_);
        omxDisp.isDirty();
        return;
    }

    encoderSelect = !encoderSelect;
    omxDisp.isDirty();
}

void OmxModeMidiKeyboard::onEncoderButtonUp()
{
    if (organelleMotherMode)
    {
        //				MM::sendControlChange(CC_OM1,0,sysSettings.midiChannel);
    }
}

void OmxModeMidiKeyboard::onEncoderButtonDownLong()
{
}

bool OmxModeMidiKeyboard::shouldBlockEncEdit()
{
    if (isSubmodeEnabled())
    {
        return activeSubmode->shouldBlockEncEdit();
    }

    if(macroActive_)
    {
        return true;
    }

    return false;
}

void OmxModeMidiKeyboard::onKeyUpdate(OMXKeypadEvent e)
{
    if (isSubmodeEnabled())
    {
        activeSubmode->onKeyUpdate(e);
        return;
    }

    int thisKey = e.key();

    // // Aux key debugging
    // if(thisKey == 0){
    //     const char* dwn = e.down() ? " Down: True" : " Down: False";
    //     Serial.println(String("Clicks: ") + String(e.clicks()) + dwn);
    // }

    // Aux double click toggle macro
    if (midiMacroConfig.midiMacro > 0)
    {
        if (!macroActive_)
        {
            // Enter M8 Mode
            if (!e.down() && thisKey == 0 && e.clicks() == 2)
            {
                midiSettings.midiAUX = false;

                activeMacro_ = getActiveMacro();
                if(activeMacro_ != nullptr)
                {
                    macroActive_ = true;
                    activeMacro_->setEnabled(true);
                    activeMacro_->setScale(musicScale);
                    omxDisp.setDirty();
                    return;
                }
                // midiMacroConfig.m8AUX = true;
                return;
            }
        }
        else // Macro mode active
        {
            if(!e.down() && thisKey == 0 && e.clicks() == 2)
            {
                // exit macro mode
                if (activeMacro_ != nullptr)
                {
                    activeMacro_->setEnabled(false);
                    activeMacro_ = nullptr;
                }

                midiSettings.midiAUX = false;
                macroActive_ = false;
                omxDisp.setDirty();

                // Clear LEDs
                for (int m = 1; m < LED_COUNT; m++)
                {
                    strip.setPixelColor(m, LEDOFF);
                }
            }
            else
            {
                if(activeMacro_ != nullptr)
                {
                    activeMacro_->onKeyUpdate(e);
                }
            }
            return;

            // if(activeMarco_->getEnabled() == false)
            // {
            //     macroActive_ = false;
            //     midiSettings.midiAUX = false;
            //     activeMarco_ = nullptr;

            //     // Clear LEDs
            //     for (int m = 1; m < LED_COUNT; m++)
            //     {
            //         strip.setPixelColor(m, LEDOFF);
            //     }
            //     return;
            // }
            // // Exit M8 mode
            // if (!e.down() && thisKey == 0 && e.clicks() == 2)
            // {
            //     midiMacroConfig.m8AUX = false;
            //     midiSettings.midiAUX = false;
            //     macroActive_ = true;

            //     // Clear LEDs
            //     for (int m = 1; m < LED_COUNT; m++)
            //     {
            //         strip.setPixelColor(m, LEDOFF);
            //     }
            //     return;
            // }

            // onKeyUpdateM8Macro(e);
            // return;
        }
    }

    // REGULAR KEY PRESSES
    if (!e.held())
    { // IGNORE LONG PRESS EVENTS
        if (e.down() && thisKey != 0)
        {
            bool keyConsumed = false; // If used for aux, key will be consumed and not send notes.

            if (midiSettings.midiAUX) // Aux mode
            {
                keyConsumed = true;

                if (thisKey == 11 || thisKey == 12) // Change Octave
                {
                    int amt = thisKey == 11 ? -1 : 1;
                    midiSettings.newoctave = constrain(midiSettings.octave + amt, -5, 4);
                    if (midiSettings.newoctave != midiSettings.octave)
                    {
                        midiSettings.octave = midiSettings.newoctave;
                    }
                }
                else if (thisKey == 1 || thisKey == 2) // Change Param selection
                {
                    if(thisKey == 1){
                        params.decrementParam();
                    }
                    else if(thisKey == 2){
                        params.incrementParam();
                    }
                    // int chng = thisKey == 1 ? -1 : 1;

                    // setParam(constrain((midiPageParams.miparam + chng) % midiPageParams.numParams, 0, midiPageParams.numParams - 1));
                }
                else if(thisKey == 5)
                {
                    // Turn off midiFx
                    mfxIndex = 127;
                }
                else if (thisKey >= 6 && thisKey < 11)
                {
                    // Change active midiFx
                    mfxIndex = thisKey - 6;
                    // enableSubmode(&subModeMidiFx[thisKey - 6]);
                }
                // else if (e.down() && thisKey == 10)
                // {
                //     enableSubmode(&subModeMidiFx);
                //     keyConsumed = true;
                // }
                // else if (thisKey == 26)
				// {
				// 	keyConsumed = true;
                // }
            }

            if(!keyConsumed)
            {
                doNoteOn(thisKey);
                // omxUtil.midiNoteOn(musicScale, thisKey, midiSettings.defaultVelocity, sysSettings.midiChannel);
            }
        }
        else if (!e.down() && thisKey != 0)
        {
            doNoteOff(thisKey);
            // omxUtil.midiNoteOff(thisKey, sysSettings.midiChannel);
        }
    }
    //				Serial.println(e.clicks());

    // AUX KEY
    if (e.down() && thisKey == 0)
    {
        // Hard coded Organelle stuff
        //					MM::sendControlChange(CC_AUX, 100, sysSettings.midiChannel);

        // if (!midiMacroConfig.m8AUX)
        // {
        //     midiSettings.midiAUX = true;
        // }

        if (!macroActive_)
        {
            midiSettings.midiAUX = true;
        }

        //					if (midiAUX) {
        //						// STOP CLOCK
        //						Serial.println("stop clock");
        //					} else {
        //						// START CLOCK
        //						Serial.println("start clock");
        //					}
        //					midiAUX = !midiAUX;
    }
    else if (!e.down() && thisKey == 0)
    {
        // Hard coded Organelle stuff
        //					MM::sendControlChange(CC_AUX, 0, sysSettings.midiChannel);
        if (midiSettings.midiAUX)
        {
            midiSettings.midiAUX = false;
        }
        // turn off leds
        strip.setPixelColor(0, LEDOFF);
        strip.setPixelColor(1, LEDOFF);
        strip.setPixelColor(2, LEDOFF);
        strip.setPixelColor(11, LEDOFF);
        strip.setPixelColor(12, LEDOFF);
    }
}

void OmxModeMidiKeyboard::onKeyHeldUpdate(OMXKeypadEvent e)
{
    int thisKey = e.key();

    if (midiSettings.midiAUX) // Aux mode
    {
        // Enter MidiFX mode
        if (thisKey >= 6 && thisKey < 11)
        {
            enableSubmode(&subModeMidiFx[thisKey - 6]);
        }
    }
}

midimacro::MidiMacroInterface *OmxModeMidiKeyboard::getActiveMacro()
{
    switch (midiMacroConfig.midiMacro)
    {
    case 1:
        return &m8Macro_;
    case 2:
        return &nornsMarco_;
    }
    return nullptr;
}

// void OmxModeMidiKeyboard::onKeyUpdateM8Macro(OMXKeypadEvent e)
// {
//     if (!macroActive_)
//         return;
//     // if (!midiMacroConfig.m8AUX)
//     //     return;

//     auto activeMacro = getActiveMacro();
//     if(activeMacro == nullptr) return;

//     activeMacro->onKeyUpdate(e);
// }

void OmxModeMidiKeyboard::updateLEDs()
{
    if (midiSettings.midiAUX)
    {
        omxLeds.updateBlinkStates();

        bool blinkState = omxLeds.getBlinkState();

        // Blink left/right keys for octave select indicators.
        auto color1 = blinkState ? LIME : LEDOFF;
        auto color2 = blinkState ? MAGENTA : LEDOFF;
        auto color3 = blinkState ? ORANGE : LEDOFF;
        auto color4 = blinkState ? RBLUE : LEDOFF;

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
        strip.setPixelColor(11, color3);
        strip.setPixelColor(12, color4);

        // MidiFX off
        strip.setPixelColor(5, (mfxIndex >= NUM_MIDIFX_GROUPS ? kSelMidiFXOffColor : kMidiFXOffColor));

        for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
        {
            auto mfxColor = (i == mfxIndex) ? kSelMidiFXColor : kMidiFXColor;

            strip.setPixelColor(6 + i, mfxColor);
        }

        // strip.setPixelColor(10, color3); // MidiFX key

        // Macros
    }
    else
    {
        omxLeds.drawMidiLeds(musicScale); // SHOW LEDS
    }
}

void OmxModeMidiKeyboard::onDisplayUpdate()
{
    if (isSubmodeEnabled())
    {
        activeSubmode->onDisplayUpdate();
        return;
    }

    bool macroConsumesDisplay = false;

    if(macroActive_ && activeMacro_ != nullptr)
    {
        activeMacro_->drawLEDs();
        macroConsumesDisplay = activeMacro_->consumesDisplay();
    }
    else
    {
        updateLEDs();

        // if (omxLeds.isDirty())
        // {
        //     updateLEDs();
        //     // omxLeds.drawMidiLeds(musicScale); // SHOW LEDS
        // }
    }

    if(macroConsumesDisplay)
    {
        activeMacro_->onDisplayUpdate();
    }
    else
    {
        if (omxDisp.isDirty())
        { // DISPLAY
            if (!encoderConfig.enc_edit)
            {
                if (params.getSelPage() == 0) // SUBMODE_MIDI
                {
                    omxDisp.clearLegends();

                    //			if (midiRoundRobin) {
                    //				displaychan = rrChannel;
                    //			}
                    omxDisp.legends[0] = "OCT";
                    omxDisp.legends[1] = "CH";
                    omxDisp.legends[2] = "CC";
                    omxDisp.legends[3] = "NOTE";
                    omxDisp.legendVals[0] = (int)midiSettings.octave + 4;
                    omxDisp.legendVals[1] = sysSettings.midiChannel;
                    omxDisp.legendVals[2] = potSettings.potVal;
                    omxDisp.legendVals[3] = midiSettings.midiLastNote;
                }
                else if (params.getSelPage() == 1) // SUBMODE_MIDI2
                {
                    omxDisp.clearLegends();

                    omxDisp.legends[0] = "RR";
                    omxDisp.legends[1] = "RROF";
                    omxDisp.legends[2] = "PGM";
                    omxDisp.legends[3] = "BNK";
                    omxDisp.legendVals[0] = midiSettings.midiRRChannelCount;
                    omxDisp.legendVals[1] = midiSettings.midiRRChannelOffset;
                    omxDisp.legendVals[2] = midiSettings.currpgm + 1;
                    omxDisp.legendVals[3] = midiSettings.currbank;
                }
                else if (params.getSelPage() == 2) // SUBMODE_MIDI3
                {
                    omxDisp.clearLegends();

                    omxDisp.legends[0] = "PBNK"; // Potentiometer Banks
                    omxDisp.legends[1] = "THRU"; // MIDI thru (usb to hardware)
                    omxDisp.legends[2] = "MCRO"; // Macro mode
                    omxDisp.legends[3] = "M-CH";
                    omxDisp.legendVals[0] = potSettings.potbank + 1;
                    omxDisp.legendVals[1] = -127;
                    if (midiSettings.midiSoftThru)
                    {
                        omxDisp.legendText[1] = "On";
                    }
                    else
                    {
                        omxDisp.legendText[1] = "Off";
                    }
                    omxDisp.legendVals[2] = -127;
                    omxDisp.legendText[2] = macromodes[midiMacroConfig.midiMacro];
                    omxDisp.legendVals[3] = midiMacroConfig.midiMacroChan;
                }
                else if (params.getSelPage() == 3) // SCALES
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
                else if (params.getSelPage() == 4) // CONFIG
                {
                    omxDisp.clearLegends();
                    omxDisp.legends[0] = "CC";
                    omxDisp.legends[1] = "";
                    omxDisp.legends[2] = "";
                    omxDisp.legends[3] = "";
                    omxDisp.legendVals[0] = -127;
                    omxDisp.legendVals[1] = -127;
                    omxDisp.legendVals[2] = -127;
                    omxDisp.legendVals[3] = -127;
                    omxDisp.legendText[0] = "CFG";
                    omxDisp.legendText[1] = "";
                    omxDisp.legendText[2] = "";
                    omxDisp.legendText[3] = "";
                }

                omxDisp.dispGenericMode2(params.getNumPages(), params.getSelPage(), params.getSelParam(), encoderSelect);
            }
        }
    }
}

// incoming midi note on
void OmxModeMidiKeyboard::inMidiNoteOn(byte channel, byte note, byte velocity)
{
    if (organelleMotherMode)
        return;

    midiSettings.midiLastNote = note;
    int whatoct = (note / 12);
    int thisKey;
    uint32_t keyColor = MIDINOTEON;

    if ((whatoct % 2) == 0)
    {
        thisKey = note - (12 * whatoct);
    }
    else
    {
        thisKey = note - (12 * whatoct) + 12;
    }
    if (whatoct == 0)
    { // ORANGE,YELLOW,GREEN,MAGENTA,CYAN,BLUE,LIME,LTPURPLE
    }
    else if (whatoct == 1)
    {
        keyColor = ORANGE;
    }
    else if (whatoct == 2)
    {
        keyColor = YELLOW;
    }
    else if (whatoct == 3)
    {
        keyColor = GREEN;
    }
    else if (whatoct == 4)
    {
        keyColor = MAGENTA;
    }
    else if (whatoct == 5)
    {
        keyColor = CYAN;
    }
    else if (whatoct == 6)
    {
        keyColor = LIME;
    }
    else if (whatoct == 7)
    {
        keyColor = CYAN;
    }
    strip.setPixelColor(midiKeyMap[thisKey], keyColor); //  Set pixel's color (in RAM)
                                                        //	dirtyPixels = true;
    strip.show();
    omxDisp.setDirty();
}

void OmxModeMidiKeyboard::inMidiNoteOff(byte channel, byte note, byte velocity)
{
    if (organelleMotherMode)
        return;

    int whatoct = (note / 12);
    int thisKey;
    if ((whatoct % 2) == 0)
    {
        thisKey = note - (12 * whatoct);
    }
    else
    {
        thisKey = note - (12 * whatoct) + 12;
    }
    strip.setPixelColor(midiKeyMap[thisKey], LEDOFF); //  Set pixel's color (in RAM)
                                                      //	dirtyPixels = true;
    strip.show();
    omxDisp.setDirty();
}

void OmxModeMidiKeyboard::SetScale(MusicScales *scale)
{
    this->musicScale = scale;
    m8Macro_.setScale(scale);
    nornsMarco_.setScale(scale);
}

void OmxModeMidiKeyboard::enableSubmode(SubmodeInterface *subMode)
{
    activeSubmode = subMode;
    activeSubmode->setEnabled(true);
    omxDisp.setDirty();
}

void OmxModeMidiKeyboard::disableSubmode()
{
    activeSubmode = nullptr;
    omxDisp.setDirty();
}

bool OmxModeMidiKeyboard::isSubmodeEnabled()
{
    if(activeSubmode == nullptr) return false;

    if(activeSubmode->isEnabled() == false){
        disableSubmode();
        return false;
    }

    return true;
}

void OmxModeMidiKeyboard::doNoteOn(uint8_t keyIndex)
{
    MidiNoteGroup noteGroup = omxUtil.midiNoteOn2(musicScale, keyIndex, midiSettings.defaultVelocity, sysSettings.midiChannel);

    if(noteGroup.noteNumber == 255) return;

    // Serial.println("doNoteOn: " + String(noteGroup.noteNumber));

    noteGroup.unknownLength = true;
    noteGroup.prevNoteNumber = noteGroup.noteNumber;

    if (mfxIndex < NUM_MIDIFX_GROUPS)
    {
        subModeMidiFx[mfxIndex].noteInput(noteGroup);
        // subModeMidiFx.noteInput(noteGroup);
    }
    else
    {
        onNotePostFX(noteGroup);
    }
}
void OmxModeMidiKeyboard::doNoteOff(uint8_t keyIndex)
{
    MidiNoteGroup noteGroup = omxUtil.midiNoteOff2(keyIndex, sysSettings.midiChannel);

    if(noteGroup.noteNumber == 255) return;

    // Serial.println("doNoteOff: " + String(noteGroup.noteNumber));

    noteGroup.unknownLength = true;
    noteGroup.prevNoteNumber = noteGroup.noteNumber;

    if (mfxIndex < NUM_MIDIFX_GROUPS)
    {
        subModeMidiFx[mfxIndex].noteInput(noteGroup);
    // subModeMidiFx.noteInput(noteGroup);
    }
    else
    {
        onNotePostFX(noteGroup);
    }
}

// // Called by a euclid sequencer when it triggers a note
// void OmxModeMidiKeyboard::onNoteTriggered(uint8_t euclidIndex, MidiNoteGroup note)
// {
//     // Serial.println("OmxModeEuclidean::onNoteTriggered " + String(euclidIndex) + " note: " + String(note.noteNumber));
    
//     subModeMidiFx.noteInput(note);

//     omxDisp.setDirty();
// }

// Called by the midiFX group when a note exits it's FX Pedalboard
void OmxModeMidiKeyboard::onNotePostFX(MidiNoteGroup note)
{
    if(note.noteOff)
    {
        // Serial.println("OmxModeMidiKeyboard::onNotePostFX noteOff: " + String(note.noteNumber));

        if (note.sendMidi)
        {
            MM::sendNoteOff(note.noteNumber, note.velocity, note.channel);
        }
        if (note.sendCV)
        {
            omxUtil.cvNoteOff();
        }
    }
    else
    {
        // Serial.println("OmxModeMidiKeyboard::onNotePostFX noteOn: " + String(note.noteNumber));

        if (note.sendMidi)
        {
            MM::sendNoteOn(note.noteNumber, note.velocity, note.channel);
        }
        if (note.sendCV)
        {
            omxUtil.cvNoteOn(note.noteNumber);
        }
    }

    // uint32_t noteOnMicros = note.noteonMicros; // TODO Might need to be set to current micros
    // pendingNoteOns.insert(note.noteNumber, note.velocity, note.channel, noteOnMicros, note.sendCV);

    // uint32_t noteOffMicros = noteOnMicros + (note.stepLength * clockConfig.step_micros);
    // pendingNoteOffs.insert(note.noteNumber, note.channel, noteOffMicros, note.sendCV);
}

void OmxModeMidiKeyboard::onPendingNoteOff(int note, int channel)
{
    // Serial.println("OmxModeEuclidean::onPendingNoteOff " + String(note) + " " + String(channel));
    // subModeMidiFx.onPendingNoteOff(note, channel);

    for(uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
    {
        subModeMidiFx[i].onPendingNoteOff(note, channel);
    }
}