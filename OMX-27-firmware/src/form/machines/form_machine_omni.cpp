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

    const uint8_t kZoomMults[] = {1,2,4};
    const uint8_t kPageMax[] = {4,2,1};

    // 0, 0.33333, 0.66666, 0.83333
    // 0, 0.25, 0.5, 0.75, 1.0
    // 0, 0.083333, 0.166666, 0.083333
    // Percent, how much to nudge note forward to become a triplet note
    const float kTripletNudge[] = {0.0f, 1.0f/3.0f, 1.0f/3.0f * 2.0f, 0.0f};

    // Mod to use for swing
    // 16th is 2
    // 0S0S0S
    // 00S00
    // 10001000100010001000
    const uint8_t kSwingDivisionMod[] = {2,4};

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

        resetPlayback();

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
        return "LEN | RATE";
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
            grooveCounter_ = 0;
            ticksTilNext16Trigger_ = 0;
            ticksTilNextTrigger_ = ticksTilNext16Trigger_;
            ticksTilNextTriggerRate_ = ticksTilNext16Trigger_;
        }
        else
        {
            grooveCounter_ = 0;
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

    uint8_t FormMachineOmni::key16toStep(uint8_t key16)
    {
        uint8_t zoomMult = kZoomMults[zoomLevel_];

        uint8_t view = 16 * zoomMult;

        uint8_t page = min(activePage_, kPageMax[zoomLevel_]);

        uint8_t stepIndex = (view * page) + (key16 * zoomMult);

        return stepIndex;
    }

    void FormMachineOmni::selStep(uint8_t stepIndex)
    {
        selStep_ = key16toStep(stepIndex);

        omniNoteEditor.setSelStep(selStep_);
    }

    void FormMachineOmni::copyStep(uint8_t keyIndex)
    {
        if(keyIndex < 0 || keyIndex >= 16) return;

        uint8_t stepIndex = key16toStep(keyIndex);

        auto track = getTrack();
        bufferedStep_.CopyFrom(&track->steps[stepIndex]);
    }
    void FormMachineOmni::cutStep(uint8_t keyIndex)
    {
        if(keyIndex < 0 || keyIndex >= 16) return;

        uint8_t stepIndex = key16toStep(keyIndex);

        copyStep(keyIndex);
        getTrack()->steps[stepIndex].setToInit();
    }
    void FormMachineOmni::pasteStep(uint8_t keyIndex)
    {
        if(keyIndex < 0 || keyIndex >= 16) return;

        uint8_t stepIndex = key16toStep(keyIndex);

        getTrack()->steps[stepIndex].CopyFrom(&bufferedStep_);
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
                case 2:
                {
                    auto track = getTrack();
                    track->swing = constrain(track->swing + amtFast, -100, 100);
                }
                break;
                case 3:
                {
                    auto track = getTrack();
                    track->swingDivision = constrain(track->swingDivision + amtSlow, 0, 1);
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
                case 2:
                {
                    auto track = getTrack();
                    track->tripletMode = constrain(track->tripletMode + amtSlow, 0, 1);
                    onRateChanged();

                }
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

        if(potIndex == 0)
        {
            activePage_ = map(newValue, 0, 127, 0, kPageMax[zoomLevel_] - 1);
            omxDisp.displayMessage("Page " + String(activePage_ + 1));
        }
        else if(potIndex == 1)
        {
            zoomLevel_ = map(newValue, 0, 127, 0, 2);
            omxDisp.displayMessage("Zoom " + String(kZoomMults[zoomLevel_]) + "x");
        }
        else if (potIndex == 4)
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

            bool shouldBeOnRate = currentStep->nudge == 0;

            // if(track->tripletMode == 1 && grooveCounter_ % 4 != 0)
            // {
            //     shouldBeOnRate = false;
            // }

            // Step should be on rate, delay until on rate
            if(shouldBeOnRate && !onRate)
            {
                ticksTilNextTrigger_ = ticksTilNextTriggerRate_;
                break;
            }

            int8_t directionIncrement = track->playDirection == TRACKDIRECTION_FORWARD ? 1 : -1;

            uint8_t nextStepIndex = (playingStep_ + length + directionIncrement) % length;

            // Skip every 4th step
            if(track->tripletMode == 1)
            {
                if(nextStepIndex % 4 == 3)
                {
                    // increment again
                    nextStepIndex = (nextStepIndex + length + directionIncrement) % length;
                }
            }
            // uint8_t nextStepIndex = (playingStep_ + directionIncrement) % length;
            auto nextStep = &track->steps[nextStepIndex];

            triggerStep(currentStep);
            lastTriggeredStepIndex = playingStep_;
            // int currentNudgeTicks = abs(currentStep->nudge)

            // Reverse the nudges when flipping directions
            int8_t nudgeCurrent = currentStep->nudge * directionIncrement;
            int8_t nudgeNext = nextStep->nudge * directionIncrement;

            // bool isSwingStep = false;
            // bool isNextSwingStep = false;
            // float swingPerc = track->swing / 100.0f;

            // if(track->swingDivision == 0) // 16th
            // {
            //     // 1SxS2SxS3SxS4SxS
            //     isSwingStep = grooveCounter_ % 2 == 1; // Swing every other 16th. Basically every even 16th
            //     isNextSwingStep = (grooveCounter_ + 1) % 2 == 1;
            // }
            // else if(track->swingDivision == 1) // 8th
            // {
            //     // 1xSx2xSx3xSx4xSx
            //     isSwingStep = grooveCounter_ % 4 == 2; // Swing ever other 8th note
            //     isNextSwingStep = (grooveCounter_ + 1) % 4 == 2;
            // }

            // float nudgePerc = abs(currentStep->nudge) / 60.0f * (currentStep->nudge < 0 ? -1 : 1);
            int nudgeTicks = (nudgeCurrent / 60.0f) * ticksPerStep_;
            // float nextNudgePerc = abs(nextStep->nudge) / 60.0f * (nextStep->nudge < 0 ? -1 : 1);
            int nextNudgeTicks = (nudgeNext / 60.0f) * ticksPerStep_;

            // Apply Swing
            // By using the nudge system
            // nudgeTicks = isSwingStep ? (nudgeTicks + (swingPerc * ticksPerStep_)) : nudgeTicks;
            // nextNudgeTicks = isNextSwingStep ? (nextNudgeTicks + (swingPerc * ticksPerStep_)) : nextNudgeTicks;

            // if(track->tripletMode == 1)
            // {
            //     uint8_t modPos = grooveCounter_ % 4;
            //     uint8_t nextModPos = (grooveCounter_ + 1) % 4;
            //     // nudgeTicks = nudgeTicks + (kTripletNudge[modPos] * ticksPerStep_);
            //     // nextNudgeTicks = nextNudgeTicks + (kTripletNudge[nextModPos] * ticksPerStep_);


            //     nudgeTicks = kTripletNudge[modPos] * ticksPerStep_;
            //     nextNudgeTicks = kTripletNudge[nextModPos] * ticksPerStep_;
            // }

            // nudgeTicks = constrain(nudgeTicks, -ticksPerStep_, ticksPerStep_);
            // nextNudgeTicks = constrain(nextNudgeTicks, -ticksPerStep_, ticksPerStep_);

            if(!onRate && nextNudgeTicks == 0)
            {
                ticksTilNextTrigger_ = ticksTilNextTriggerRate_;
            }
            else
            {
                ticksTilNextTrigger_ = ticksPerStep_ + nextNudgeTicks - nudgeTicks;
            }

            if(nudgeTicks < 0 && !onRate)
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

            // always counts forward
            grooveCounter_ = (grooveCounter_ + 1) % 16;
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

        auto track = getTrack();

        ticksPerStep_ = roundf((PPQ * 4) / (float)rate);

        if(track->tripletMode == 1)
        {
            ticksPerStep_ = ticksPerStep_ * 4 / 3.0f;
        }

        // ticksTilNextTrigger_ = 0; // Should we reset this?

        ticksTilNextTrigger_ = ticksTilNext16Trigger_;
        ticksTilNextTriggerRate_ = ticksTilNext16Trigger_;

        stepLengthMult_ = 16.0f / rate;

        stepMicros_ = clockConfig.step_micros * 16 / rate;

        if(track->tripletMode == 1)
        {
            stepMicros_ = stepMicros_ * 4 / 3;
        }
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
                uint8_t stepIndex = key16toStep(i);
                bool isInLen = stepIndex <= track->len;
                auto step = &track->steps[stepIndex];
                int keyColor = (step->mute || !isInLen) ? LEDOFF : (step->hasNotes() ? LTBLUE : DKBLUE);
                strip.setPixelColor(11 + i, keyColor);
            }

            if(omxFormGlobal.isPlaying)
            {
                uint8_t playingStepKey = lastTriggeredStepIndex % (16 * kZoomMults[zoomLevel_]);

                playingStepKey = map(playingStepKey, 0, 16 * kZoomMults[zoomLevel_], 0, 15);

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
                        uint8_t stepIndex = key16toStep(thisKey - 11);
                        track->steps[stepIndex].mute = !track->steps[stepIndex].mute;
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
                    omxDisp.displayMessage("RATE 1:" + String(kSeqRates[seq_.rate]));
                    onRateChanged();
                }
                if (e.down() && thisKey >= 11 && thisKey < 27)
                {
                    auto track = getTrack();
                    track->len = (thisKey - 11) * kZoomMults[zoomLevel_] + (kZoomMults[zoomLevel_] - 1);

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

                auto track = getTrack();

                omxDisp.setLegend(2, "SWNG", track->swing);
                omxDisp.setLegend(3, "S-DV", track->swingDivision == 0 ? "16th" : "8th");
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
                auto track = getTrack();

                omxDisp.setLegend(0, "TPOS", 100);
                omxDisp.setLegend(1, "TYPE", seq_.transposeMode == 0 ? "INTR" : "SEMI");
                omxDisp.setLegend(2, "TRIP", track->tripletMode == 0 ? "OFF" : "ON");
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