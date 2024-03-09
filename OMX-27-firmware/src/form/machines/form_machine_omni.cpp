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

    void FormMachineOmni::playBackStateChanged(bool newIsPlaying)
    {
        if(newIsPlaying)
        {
            nextStepTime_ = seqConfig.lastClockMicros;
            playingStep_ = 0;
            ticksTilNextTrigger_ = 0;
            onRateChanged();

            // Calculate first step

        }
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

    void FormMachineOmni::triggerStep(Step *step)
    {
        if(context_ == nullptr || noteOnFuncPtr == nullptr)
            return;

        if((bool)step->mute) return;

        Micros now = micros();

        for(int8_t i = 0; i < 6; i++)
        {
            int8_t noteNumber = step->notes[i];

            if(noteNumber >= 0 && noteNumber <= 127)
            {
                Serial.println("triggerStep: " + String(noteNumber));
                MidiNoteGroup noteGroup;
                noteGroup.channel = seq_.channel;
                noteGroup.noteNumber = noteNumber;
                noteGroup.velocity = step->vel;
                noteGroup.stepLength = 0.25f;
                noteGroup.sendMidi = (bool)seq_.sendMidi;
                noteGroup.sendCV = (bool)seq_.sendCV;
                noteGroup.noteonMicros = now;
                noteGroup.unknownLength = false;

                seqNoteOn(noteGroup, 255);
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

        if(ticksTilNextTriggerRate_ <= 0)
        {
            ticksTilNextTriggerRate_ = ticksPerStep_;
        }

        // can trigger twice in once clock if note is fully nudged
        while(ticksTilNextTrigger_ <= 0)
        {
            auto track = getTrack();
            uint8_t length = track->len + 1;

            auto currentStep = &track->steps[playingStep_];

            uint8_t nextStepIndex = (playingStep_ + 1) % length;
            auto nextStep = &track->steps[nextStepIndex];

            triggerStep(currentStep);
            // int currentNudgeTicks = abs(currentStep->nudge)

            // float nudgePerc = abs(currentStep->nudge) / 60.0f * (currentStep->nudge < 0 ? -1 : 1);
            float nudgePerc = currentStep->nudge / 60.0f;
            int nudgeTicks = nudgePerc * ticksPerStep_;

            // float nextNudgePerc = abs(nextStep->nudge) / 60.0f * (nextStep->nudge < 0 ? -1 : 1);
            float nextNudgePerc = nextStep->nudge / 60.0f;
            int nextNudgeTicks = nextNudgePerc * ticksPerStep_;

            // 24 + 16 + 16
            // ticksTilNextTrigger_ = ticksPerStep_ + nextNudgeTicks - nudgeTicks;
            ticksTilNextTrigger_ = ticksTilNextTriggerRate_ + nextNudgeTicks;
            // ticksTilNextTrigger_ = ticksPerStep_;

            playingStep_ = nextStepIndex;
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
                int keyColor = (track->steps[i].mute || !isInLen) ? LEDOFF : BLUE;
                strip.setPixelColor(11 + i, keyColor);
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
            if (e.down() && thisKey >= 13 && thisKey < 19)
            {
                changeUIMode(thisKey - 13, false);
                return true;
            }
        }

        switch (omniUiMode_)
        {
        case OMNIUIMODE_CONFIG:
        case OMNIUIMODE_MIX:
        {
            if (e.down() && thisKey >= 11 && thisKey < 27)
            {
                auto track = getTrack();

                selStep(thisKey - 11);
                track->steps[thisKey - 11].mute = !track->steps[thisKey - 11].mute;
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

            }
            break;
            case OMNIPAGE_1: // Velocity, Channel, Rate, Gate
            {
                auto track = getTrack();
                omxDisp.setLegend(0, "LEN", track->len + 1);
                omxDisp.setLegend(1, "CHAN", seq_.channel + 1);
                omxDisp.setLegend(2, "RATE", "1/" + String(kSeqRates[seq_.rate]));
                omxDisp.setLegend(3, "GATE", seq_.gate);
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