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

    Track *FormMachineOmni::getTrack()
    {
        return &seq_.tracks[0];
    }

    void FormMachineOmni::selStep(uint8_t stepIndex)
    {
        selStep_ = stepIndex;

        omniNoteEditor.setSelStep(selStep_);
    }

    void FormMachineOmni::onEnabled()
    {
    }
    void FormMachineOmni::onDisabled()
    {
    }

    void FormMachineOmni::onEncoderChangedSelectParam(Encoder::Update enc)
    {
        trackParams_.changeParam(enc.dir());
        omxDisp.setDirty();
    }
    void FormMachineOmni::onEncoderChangedEditParam(Encoder::Update enc)
    {
        int amtSlow = enc.accel(1);
        int amtFast = enc.accel(5);

        int8_t selPage = trackParams_.getSelPage();
        int8_t selParam = trackParams_.getSelParam();

        switch (selPage)
        {
        case OMNIPAGE_GBL1: // BPM
        {
        }
        break;
        case OMNIPAGE_1: // Velocity, Channel, Rate, Gate
        {
            switch (selParam)
            {
            case 0:
                break;
            case 1:
                seq_.channel = constrain(seq_.channel + amtSlow, 0, 15);
                break;
            case 2:
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

        omxDisp.setDirty();
    }
    void FormMachineOmni::onUIModeChanged(uint8_t prevMode, uint8_t newMode)
    {
        // Tell Note editor it's been started for step mode
    }

    void FormMachineOmni::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
    {
        if(potIndex == 4)
        {
            uint8_t newUIMode = map(newValue, 0, 127, 0, OMNIUIMODE_COUNT - 1);
            if(newUIMode != omniUiMode_)
            {
                onUIModeChanged(omniUiMode_, newUIMode);
                omniUiMode_ = newUIMode;
                omxDisp.displayMessage(kUIModeMsg[omniUiMode_]);
            }
        }
    }
    void FormMachineOmni::onClockTick()
    {
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
                int keyColor = track->steps[i].mute ? LEDOFF : BLUE;
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

        uint8_t thisKey = e.key();

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

        omxDisp.setDirty();
        omxLeds.setDirty();

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
            }
            break;
            case OMNIPAGE_1: // Velocity, Channel, Rate, Gate
            {
                omxDisp.setLegend(0, "VEL", 100);
                omxDisp.setLegend(1, "CHAN", seq_.channel + 1);
                omxDisp.setLegend(2, "RATE", 1);
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