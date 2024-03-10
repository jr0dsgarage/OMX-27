#include "form_machine_omni.h"
#include "../../config.h"
#include "../../consts/colors.h"
#include "../../utils/omx_util.h"
#include "../../hardware/omx_disp.h"
#include "../../hardware/omx_leds.h"
#include "omni_note_editor.h"

namespace FormOmni
{
    enum OmniPage
    {
        OMNIPAGE_GBL1, // BPM
        OMNIPAGE_1,    // Velocity, Channel, Rate, Gate
        OMNIPAGE_2,    // Transpose, TransposeMode
        OMNIPAGE_3,    // SendMidi, SendCV
        OMNIPAGE_TPAT, // SendMidi, SendCV
        OMNIPAGE_COUNT
    };

    enum OmniStepPage
    {
        OSTEPPAGE_1,
        OSTEPAGE_COUNT
    };

    const char* kUIModeMsg[] = {"CONFIG", "MIX", "LENGTH", "TRANSPOSE", "STEP", "NOTE EDIT"};

    // kSeqRates[] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
    // 1, 2, 3, 4, 8, 16, 32, 64
    const uint8_t kRateShortcuts[] = {0, 1, 2, 3, 6, 9, 12, 15};

    // Global param management so pages are same across machines
    ParamManager trackParams_;
    ParamManager noteParams_;

    bool paramsInit_ = false;

    uint8_t omniUiMode_ = 0;

    FormMachineOmni::FormMachineOmni()
    {
        if (!paramsInit_)
        {
            trackParams_.addPages(OMNIPAGE_COUNT);
        }

        onRateChanged();
    }
    FormMachineOmni::~FormMachineOmni()
    {
    }



    FormMachineInterface *FormMachineOmni::getClone()
    {
        auto clone = new FormMachineOmni();
        return clone;
    }

    bool FormMachineOmni::doesConsumePots()
    {
        return true;
        // switch (omniUiMode_)
        // {
        // case OMNIUIMODE_CONFIG:
        // case OMNIUIMODE_MIX:
        // case OMNIUIMODE_LENGTH:
        // case OMNIUIMODE_TRANSPOSE:
        // case OMNIUIMODE_COUNT:
        //     return false;
        // case OMNIUIMODE_STEP:
        // case OMNIUIMODE_NOTEEDIT:
        //     return true;
        // }
        // return false;
    }

    bool FormMachineOmni::doesConsumeDisplay()
    {
        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        case OMNIUIMODE_LENGTH:
        case OMNIUIMODE_COUNT:
            return false;
        case OMNIUIMODE_TRANSPOSE:
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
            return true;
        }
        return false;
    }

    bool FormMachineOmni::doesConsumeKeys()
    {
        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        case OMNIUIMODE_LENGTH:
        case OMNIUIMODE_COUNT:
            return false;
        case OMNIUIMODE_TRANSPOSE:
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
            return true;
        }
        return false;
    }
    bool FormMachineOmni::doesConsumeLEDs()
    {
        return doesConsumeKeys();
    }

    const char *FormMachineOmni::getF3shortcutName()
    {
        return "LEN / RATE";
    }

    void FormMachineOmni::setTest()
    {
        auto track = getTrack();

        track->len = 3;
        track->steps[0].notes[0] = 60;
        track->steps[1].notes[0] = 64;
        track->steps[2].notes[0] = 67;
        track->steps[3].notes[0] = 71;
    }

    void FormMachineOmni::playBackStateChanged(bool newIsPlaying)
    {
        if(newIsPlaying)
        {
            noteOns_.clear();

            ticksTilNext16Trigger_ = 0;
            
            // nextStepTime_ = seqConfig.lastClockMicros;
            // playingStep_ = 0;
            // ticksTilNextTrigger_ = 0;
            // ticksTilNextTriggerRate_ = 0;

            onRateChanged();

            // Calculate first step
        }
        else
        {
            for(auto n : noteOns_)
            {
                seqNoteOff(n, 255);
            }
            noteOns_.clear();
        }
    }

    void FormMachineOmni::resetPlayback()
    {
        auto track = getTrack();

        // nextStepTime_ = seqConfig.lastClockMicros + ;
        playingStep_ = track->playDirection == TRACKDIRECTION_FORWARD ? 0 : track->getLength() - 1;

        if (omxFormGlobal.isPlaying)
        {
            ticksTilNext16Trigger_ = 0;
            ticksTilNextTrigger_ = ticksTilNext16Trigger_;
            ticksTilNextTriggerRate_ = ticksTilNext16Trigger_;
        }
        else
        {
            ticksTilNextTrigger_ = 0;
            ticksTilNext16Trigger_ = 0;
            ticksTilNextTriggerRate_ = 0;
        }

        onRateChanged();
    }


    Track *FormMachineOmni::getTrack()
    {
        return &seq_.tracks[0];
    }

    Step *FormMachineOmni::getSelStep()
    {
        return &getTrack()->steps[selStep_];
    }

    void FormMachineOmni::selStep(uint8_t stepIndex)
    {
        selStep_ = stepIndex;

        omniNoteEditor.setSelStep(selStep_);
    }

    void FormMachineOmni::copyStep(uint8_t keyIndex)
    {
        if(keyIndex < 0 || keyIndex >= 16) return;

        auto track = getTrack();
        bufferedStep_.CopyFrom(&track->steps[keyIndex]);
    }
    void FormMachineOmni::cutStep(uint8_t keyIndex)
    {
        if(keyIndex < 0 || keyIndex >= 16) return;

        copyStep(keyIndex);
        getTrack()->steps[keyIndex].setToInit();
    }
    void FormMachineOmni::pasteStep(uint8_t keyIndex)
    {
        if(keyIndex < 0 || keyIndex >= 16) return;

        getTrack()->steps[keyIndex].CopyFrom(&bufferedStep_);
    }

    MidiNoteGroup FormMachineOmni::step2NoteGroup(uint8_t noteIndex, Step *step)
    {
        MidiNoteGroup noteGroup;

        noteGroup.channel = seq_.channel;

        if(noteIndex >= 6)
        {
            noteGroup.noteNumber = 255;
        }
        else
        {
            noteGroup.noteNumber = step->notes[noteIndex];
        }
        noteGroup.velocity = step->vel;

        // noteGroup.stepLength = getStepLenMult(step->len) * stepLengthMult_ * getGateMult(seq_.gate);

        float lenMult = getStepLenMult(step->len);
        // if(lenMult <= 1.0f)
        // {
        //     lenMult *= getGateMult(seq_.gate);
        // }
        noteGroup.stepLength = lenMult * getGateMult(seq_.gate);

        noteGroup.sendMidi = (bool)seq_.sendMidi;
        noteGroup.sendCV = (bool)seq_.sendCV;
        noteGroup.unknownLength = true;

        return noteGroup;
    }

    void FormMachineOmni::triggerStep(Step *step)
    {
        if(context_ == nullptr || noteOnFuncPtr == nullptr)
            return;

        if((bool)step->mute) return;

        // Micros now = micros();

        for(int8_t i = 0; i < 6; i++)
        {
            int8_t noteNumber = step->notes[i];

            if(noteNumber >= 0 && noteNumber <= 127)
            {
                // Serial.println("triggerStep: " + String(noteNumber));

                auto noteGroup = step2NoteGroup(i, step);

                bool noteTriggeredOnSameStep = false; 

                // With nudge, two steps could fire at once, 
                // If a step already triggered a note, 
                // don't trigger same note again to avoid
                // overlapping note ons
                for(auto n : triggeredNotes_)
                {
                    if(n.noteNumber == noteNumber)
                    {
                        noteTriggeredOnSameStep = true;
                        break;
                    }
                }

                if (!noteTriggeredOnSameStep)
                {
                    // bool foundNoteToRemove = false;
                    auto it = noteOns_.begin();
                    while (it != noteOns_.end())
                    {
                        // remove matching note numbers
                        if (it->noteNumber == noteNumber)
                        {
                            seqNoteOff(*it, 255);
                            // `erase()` invalidates the iterator, use returned iterator
                            it = noteOns_.erase(it);
                            // foundNoteToRemove = true;
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }

                if(!noteTriggeredOnSameStep && noteOns_.size() < 16)
                {
                    noteGroup.noteonMicros = seqConfig.lastClockMicros;
                    seqNoteOn(noteGroup, 255);
                    triggeredNotes_.push_back(noteGroup);
                    noteOns_.push_back(noteGroup);
                }
            }
        }
    }

    void FormMachineOmni::onEnabled()
    {
    }
    void FormMachineOmni::onDisabled()
    {
    }

    void FormMachineOmni::onEncoderChangedSelectParam(Encoder::Update enc)
    {
        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        case OMNIUIMODE_LENGTH:
            trackParams_.changeParam(enc.dir());
            break;
        case OMNIUIMODE_TRANSPOSE:
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
            break;
        }
        
        omxDisp.setDirty();
    }
    void FormMachineOmni::onEncoderChangedEditParam(Encoder::Update enc)
    {
        int amtSlow = enc.accel(1);
        int amtFast = enc.accel(5);

        int8_t selPage = trackParams_.getSelPage();
        int8_t selParam = trackParams_.getSelParam();

        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        {
            switch (selPage)
            {
            case OMNIPAGE_GBL1: // BPM
            {
                switch (selParam)
                {
                case 0:
                {
                    auto selStep = getSelStep();
                    selStep->nudge = constrain(selStep->nudge + amtSlow, -60, 60);
                }
                break;
                case 1:
                {
                    auto selStep = getSelStep();
                    selStep->len = constrain(selStep->len + amtSlow, 0, 22);
                }
                break;
                }
            }
            break;
            case OMNIPAGE_1: // Velocity, Channel, Rate, Gate
            {
                switch (selParam)
                {
                case 0:
                {
                    auto track = getTrack();
                    track->len = constrain(track->len + amtSlow, 0, 63);
                }
                break;
                case 1:
                    seq_.channel = constrain(seq_.channel + amtSlow, 0, 15);
                    break;
                case 2:
                    seq_.rate = constrain(seq_.rate + amtSlow, 0, kNumSeqRates - 1);
                    onRateChanged();
                    break;
                case 3:
                    seq_.gate = constrain(seq_.gate + amtFast, 0, 100);
                    break;
                }
            }
            break;
            case OMNIPAGE_2: // Transpose, TransposeMode
            {
                switch (selParam)
                {
                case 0:
                    seq_.transpose = constrain(seq_.transpose + amtSlow, -64, 64);
                    break;
                case 1:
                    seq_.transposeMode = constrain(seq_.transposeMode + amtSlow, 0, 1);
                    break;
                }
            }
            break;
            case OMNIPAGE_3: // SendMidi, SendCV
            {
                switch (selParam)
                {
                case 0:
                    seq_.sendMidi = constrain(seq_.sendMidi + amtSlow, 0, 1);
                    break;
                case 1:
                    seq_.sendCV = constrain(seq_.sendCV + amtSlow, 0, 1);
                    break;
                }
            }
            break;
            case OMNIPAGE_TPAT: // SendMidi, SendCV
            {
            }
            break;
            }
        }
        break;
        case OMNIUIMODE_LENGTH:
        case OMNIUIMODE_TRANSPOSE:
            break;
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
        {
            omniNoteEditor.onEncoderChangedEditParam(enc, getTrack());
        }
        break;
        }

        omxDisp.setDirty();
    }

    void FormMachineOmni::changeUIMode(uint8_t newMode, bool silent)
    {
        if (newMode >= OMNIUIMODE_COUNT)
            return;

        if (newMode != omniUiMode_)
        {
            uint8_t prevMode = omniUiMode_;
            omniUiMode_ = newMode;
            onUIModeChanged(prevMode, newMode);

            encoderSelect_ = true;

            if (!silent)
            {
                omxDisp.displayMessage(kUIModeMsg[omniUiMode_]);
            }
        }
    }

    void FormMachineOmni::onUIModeChanged(uint8_t prevMode, uint8_t newMode)
    {
        // Tell Note editor it's been started for step mode
    }

    void FormMachineOmni::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
    {
        // Serial.println("onPotChanged: " + String(potIndex) + " " + String(prevValue) + " " + String(newValue));

        if (potIndex == 4)
        {
            uint8_t newUIMode = map(newValue, 0, 127, 0, OMNIUIMODE_COUNT - 1);

            changeUIMode(newUIMode, false);
        }
    }

    void FormMachineOmni::onClockTick()
    {
        if(omxFormGlobal.isPlaying == false) return;

        // Send note offs
        auto it = noteOns_.begin();
        while (it != noteOns_.end())
        {
            Micros noteOffMicros = it->noteonMicros + (stepMicros_ * it->stepLength);
            // remove matching note numbers
            if (seqConfig.lastClockMicros >= noteOffMicros)
            {
                seqNoteOff(*it, 255);
                it = noteOns_.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // currentClockTick goes up to 1 bar, 96 * 4 = 384

        // ticksPerStep_
        // 1 = 384, 3 = 192, 4 = 128, 4 = 96, 5 = 76.8, 6 = 64, 8 = 48, 10 = 38.4, 12 = 32, 16 = 24, 20 = 19.2, 24 = 16, 32 = 12, 40 = 9.6, 48 = 8, 64 = 6

        // int16_t maxTick = PPQ * 4; // 384

        // int16_t maxTick = ticksPerStep_ * 16; // 384
        // 

        // if(seqConfig.currentClockTick % 384 == 0)
        // {

        //     omniTick_++;
        // }

        // 1xxxxx1xxxxx1xxxxx1xxxxx1xxxxx1xxxxx1xxxxx

        // 0543210543210
        // 1xxxxx1xxxxx1

        if(ticksTilNext16Trigger_ <= 0)
        {
            ticksTilNext16Trigger_ = 24;
        }

        bool onRate = false;

        if(ticksTilNextTriggerRate_ <= 0)
        {
            ticksTilNextTriggerRate_ = ticksPerStep_;

            onRate = true;
        }

        // 1xxxxx1xxxxx1xxxxx1xxxxx1xxxxx1xxxxx
        // 0543210543210
        // 1xx1xxxxxxxx1xxxxx1
        // 0210876543210543210
        // 5-3, 3 + 6 - 1

        // uint8_t loop = 0;


        triggeredNotes_.clear();

        // can trigger twice in once clock if note is fully nudged
        while(ticksTilNextTrigger_ <= 0)
        {
            auto track = getTrack();
            uint8_t length = track->len + 1;

            auto currentStep = &track->steps[playingStep_];

            // Step should be on rate, delay until on rate
            if(currentStep->nudge == 0 && !onRate)
            {
                ticksTilNextTrigger_ = ticksTilNextTriggerRate_;
                break;
            }

            int8_t directionIncrement = track->playDirection == TRACKDIRECTION_FORWARD ? 1 : -1;

            uint8_t nextStepIndex = (playingStep_ + length + directionIncrement) % length;
            // uint8_t nextStepIndex = (playingStep_ + directionIncrement) % length;
            auto nextStep = &track->steps[nextStepIndex];

            triggerStep(currentStep);
            lastTriggeredStepIndex = playingStep_;
            // int currentNudgeTicks = abs(currentStep->nudge)

            // Reverse the nudges when flipping directions
            int8_t nudgeCurrent = currentStep->nudge * directionIncrement;
            int8_t nudgeNext = nextStep->nudge * directionIncrement;

            // float nudgePerc = abs(currentStep->nudge) / 60.0f * (currentStep->nudge < 0 ? -1 : 1);
            int nudgeTicks = (nudgeCurrent / 60.0f) * ticksPerStep_;

            // float nextNudgePerc = abs(nextStep->nudge) / 60.0f * (nextStep->nudge < 0 ? -1 : 1);
            int nextNudgeTicks = (nudgeNext / 60.0f) * ticksPerStep_;

            if(!onRate && nudgeNext == 0)
            {
                ticksTilNextTrigger_ = ticksTilNextTriggerRate_;
            }
            else
            {
                ticksTilNextTrigger_ = ticksPerStep_ + nextNudgeTicks - nudgeTicks;
            }

            if(nudgeCurrent < 0 && !onRate)
            {
                ticksTilNextTrigger_ += ticksPerStep_;
            }


            // if(currentStep->nudge >=0 && nextStep->nudge < 0)
            // {
            //     if (onRate)
            //     {
            //         ticksTilNextTrigger_ = ticksPerStep_ + nextNudgeTicks - nudgeTicks;
            //     }
            //     else
            //     {
            //         ticksTilNextTrigger_ = ticksTilNextTriggerRate_ + nextNudgeTicks;
            //     }
            // }






            // if (onRate)
            // {
            //     ticksTilNextTrigger_ = ticksPerStep_ + nextNudgeTicks - nudgeTicks;
            // }
            // else
            // {
            //     ticksTilNextTrigger_ = ticksTilNextTriggerRate_ + nextNudgeTicks;
            //     // if (currentStep->nudge < 0)
            //     // {
            //     //     ticksTilNextTrigger_ = ticksTilNextTriggerRate_ + ticksPerStep_ + nextNudgeTicks - 1;
            //     // }
            //     // else
            //     // {
            //     //     ticksTilNextTrigger_ = ticksTilNextTriggerRate_ + nextNudgeTicks;
            //     // }
            // }

                // 24 + 16 + 16
            // ticksTilNextTrigger_ = ticksPerStep_ + nextNudgeTicks - nudgeTicks;
            // ticksTilNextTrigger_ = ticksTilNextTriggerRate_ + nextNudgeTicks;
            // ticksTilNextTrigger_ = ticksPerStep_;

            // loop++;

            playingStep_ = nextStepIndex;
            omxLeds.setDirty();
        }

        ticksTilNextTrigger_--;
        ticksTilNext16Trigger_--;
        ticksTilNextTriggerRate_--;

        // if (seqConfig.lastClockMicros >= nextStepTime_)
        // {
        //     // 96 ppq

        //     auto track = getTrack();

        //     uint8_t length = track->len + 1;


        //     // uint8_t nextStepIndex = playingStep_ + 1 % length;

        //     auto playingStep = &track->steps[playingStep_];
        //     // auto nextStep = &track->steps[nextStepIndex];

        //     triggerStep(playingStep);

        //     nextStepTime_ = nextStepTime_ + clockConfig.ppqInterval * ticksPerStep_;
        //     playingStep_ = (playingStep_ + 1) % length;
        // }

        // seqConfig.lastClockMicros = ;
        // if(seqConfig.currentClockTick % 96 == 0)
        // {

        // }
        // seqConfig.currentClockTick

        // clockConfig.ppqInterval
    }

    void FormMachineOmni::onRateChanged()
    {
        int8_t rate = kSeqRates[seq_.rate];
        ticksPerStep_ = roundf((PPQ * 4) / (float)rate);

        // ticksTilNextTrigger_ = 0; // Should we reset this?

        ticksTilNextTrigger_ = ticksTilNext16Trigger_;
        ticksTilNextTriggerRate_ = ticksTilNext16Trigger_;

        stepLengthMult_ = 16.0f / rate;

        stepMicros_ = clockConfig.step_micros * 16 / rate;
    }

    float FormMachineOmni::getStepLenMult(uint8_t len)
    {
        float lenMult = 1.0f;

        switch (len)
        {
        case 0:
            lenMult = 0.125f;
            break;
        case 1:
            lenMult = 0.25f;
            break;
        case 2:
            lenMult = 0.5f;
            break;
        case 3:
            lenMult = 0.75f;
            break;
        case 20: // 2 bar
            lenMult = 16 * 2;
            break;
        case 21:
            lenMult = 16 * 3;
            break;
        case 22: // 4 bar
            lenMult = 16 * 4;
            break;
        default:
            lenMult = len - 3;
            break;
        }

        return lenMult;
    }

    float FormMachineOmni::getGateMult(uint8_t gate)
    {
        return max(gate / 100.f * 2, 0.01f);
    }

    void FormMachineOmni::loopUpdate()
    {
    }
    bool FormMachineOmni::updateLEDs()
    {

        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        {
            auto track = getTrack();

            for (uint8_t i = 0; i < 16; i++)
            {
                bool isInLen = i <= track->len;
                int keyColor = (track->steps[i].mute || !isInLen) ? LEDOFF : (track->steps[i].hasNotes() ? LTBLUE : DKBLUE);
                strip.setPixelColor(11 + i, keyColor);
            }

            if(omxFormGlobal.isPlaying)
            {
                uint8_t playingStepKey = lastTriggeredStepIndex % 16;

                strip.setPixelColor(11 + playingStepKey, WHITE);
            }
        }
        break;
        case OMNIUIMODE_LENGTH:
        break;
        case OMNIUIMODE_TRANSPOSE:
        break;
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
            omniNoteEditor.updateLEDs(getTrack());
            break;
        }

        return true;
    }
    void FormMachineOmni::onEncoderButtonDown()
    {
    }
    bool FormMachineOmni::onKeyUpdate(OMXKeypadEvent e)
    {
        if (e.held())
            return false;

        omxDisp.setDirty();
        omxLeds.setDirty();

        uint8_t thisKey = e.key();

        if (omxFormGlobal.shortcutMode == FORMSHORTCUT_AUX)
        {
            if(e.down())
            {
                if(thisKey == 3)
                {
                    auto track = getTrack();
                    track->playDirection = track->playDirection == TRACKDIRECTION_FORWARD ? TRACKDIRECTION_REVERSE : TRACKDIRECTION_FORWARD;
                    omxDisp.displayMessage(track->playDirection == TRACKDIRECTION_FORWARD ? ">>" : "<<");
                }
                else if (thisKey >= 13 && thisKey < 19)
                {
                    changeUIMode(thisKey - 13, false);
                    return true;
                }
            }
        }

        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        {
            switch (omxFormGlobal.shortcutMode)
            {
            case FORMSHORTCUT_NONE:
            {
                if (thisKey >= 11 && thisKey < 27)
                {
                    if (e.quickClicked())
                    {
                        auto track = getTrack();

                        selStep(thisKey - 11);
                        track->steps[thisKey - 11].mute = !track->steps[thisKey - 11].mute;
                    }
                    else if (e.down())
                    {
                        selStep(thisKey - 11);
                    }
                }
            }
            break;
            case FORMSHORTCUT_AUX:
                break;
            case FORMSHORTCUT_F1:
                // Copy Paste
                if (e.down() && thisKey >= 11 && thisKey < 27)
                {
                    if (omxFormGlobal.shortcutPaste == false)
                    {
                        copyStep(thisKey - 11);
                        omxFormGlobal.shortcutPaste = true;
                    }
                    else
                    {
                        pasteStep(thisKey - 11);
                    }
                }
                break;
            case FORMSHORTCUT_F2:
                // Cut Paste
                if (e.down() && thisKey >= 11 && thisKey < 27)
                {
                    if (omxFormGlobal.shortcutPaste == false)
                    {
                        cutStep(thisKey - 11);
                        omxFormGlobal.shortcutPaste = true;
                    }
                    else
                    {
                        pasteStep(thisKey - 11);
                    }
                }
                break;
            case FORMSHORTCUT_F3:
                // Set track length
                if(e.down() && thisKey >= 3 && thisKey <= 10)
                {
                    seq_.rate = kRateShortcuts[thisKey - 3];
                    omxDisp.displayMessage("RATE 1/" + String(kSeqRates[seq_.rate]));
                }
                if (e.down() && thisKey >= 11 && thisKey < 27)
                {
                    auto track = getTrack();
                    track->len = thisKey - 11;
                }
                break;
            }
        }
        break;
        case OMNIUIMODE_LENGTH:
        break;
        case OMNIUIMODE_TRANSPOSE:
        break;
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
            omniNoteEditor.onKeyUpdate(e, getTrack());
            break;
        }
        return false;
    }
    bool FormMachineOmni::onKeyHeldUpdate(OMXKeypadEvent e)
    {
        return true;
    }
    void FormMachineOmni::onDisplayUpdate()
    {
        omxDisp.clearLegends();

        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        case OMNIUIMODE_LENGTH:
        {
            int8_t selPage = trackParams_.getSelPage();

            switch (selPage)
            {
            case OMNIPAGE_GBL1: // BPM
            {
                auto selStep = getSelStep();

                int8_t nudgePerc = (selStep->nudge / 60.0f) * 100;
                omxDisp.setLegend(0, "NUDG", nudgePerc);

                float stepLenMult = getStepLenMult(selStep->len);

                if(stepLenMult < 1.0f)
                {
                    omxDisp.setLegend(1, "LEN", String(stepLenMult,2));
                }
                else if(stepLenMult > 16)
                {
                    uint8_t bar = stepLenMult / 16.0f;
                    omxDisp.setLegend(1, "LEN", String(bar)+"br");
                }
                else
                {
                    omxDisp.setLegend(1, "LEN", String(stepLenMult, 0));
                }
            }
            break;
            case OMNIPAGE_1: // Velocity, Channel, Rate, Gate
            {
                auto track = getTrack();
                omxDisp.setLegend(0, "LEN", track->len + 1);
                omxDisp.setLegend(1, "CHAN", seq_.channel + 1);
                omxDisp.setLegend(2, "RATE", "1/" + String(kSeqRates[seq_.rate]));
                uint8_t gateMult = getGateMult(seq_.gate) * 100;
                omxDisp.setLegend(3, "GATE", gateMult);
            }
            break;
            case OMNIPAGE_2: // Transpose, TransposeMode
            {
                omxDisp.setLegend(0, "TPOS", 100);
                omxDisp.setLegend(1, "TYPE", seq_.transposeMode == 0 ? "INTR" : "SEMI");
            }
            break;
            case OMNIPAGE_3: // SendMidi, SendCV
            {
                omxDisp.setLegend(0, "MIDI", seq_.sendMidi ? "SEND" : "OFF");
                omxDisp.setLegend(1, "CV", seq_.sendCV ? "SEND" : "OFF");
            }
            break;
            case OMNIPAGE_TPAT: // SendMidi, SendCV
            {
            }
            break;
            }

            omxDisp.dispGenericMode2(trackParams_.getNumPages(), trackParams_.getSelPage(), trackParams_.getSelParam(), getEncoderSelect());
        }
        break;
        case OMNIUIMODE_TRANSPOSE:
            break;
        case OMNIUIMODE_STEP:
        case OMNIUIMODE_NOTEEDIT:
            omniNoteEditor.onDisplayUpdate(getTrack());
            break;
        }
    }

    // AUX + Top 1 = Play Stop
    // For Omni:
    // AUX + Top 2 = Reset
    // AUX + Top 3 = Flip play direction if forward or reverse
    // AUX + Top 4 = Increment play direction mode
    void FormMachineOmni::onAUXFunc(uint8_t funcKey) {}
}