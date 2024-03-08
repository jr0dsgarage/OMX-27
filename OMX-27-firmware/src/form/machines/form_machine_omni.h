#pragma once
#include "form_machine_interface.h"
#include "omni_structs.h"

namespace FormOmni
{

    // Very powerful step sequencer
    class FormMachineOmni : public FormMachineInterface
    {
    public:
        FormMachineOmni();
        ~FormMachineOmni();

        FormMachineType getType() { return FORMMACH_OMNI; }
        FormMachineInterface *getClone() override;

        bool doesConsumePots() override;
        bool doesConsumeDisplay() override;
        bool doesConsumeKeys() override; 
        bool doesConsumeLEDs() override; 

        // Standard Updates
        void onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta) override;
        void onClockTick() override;
        void loopUpdate() override;
        bool updateLEDs() override;
        void onEncoderButtonDown() override;
        bool onKeyUpdate(OMXKeypadEvent e) override;
        bool onKeyHeldUpdate(OMXKeypadEvent e) override;
        void onDisplayUpdate() override;

        // AUX + Top 1 = Play Stop
        // For Omni:
        // AUX + Top 2 = Reset
        // AUX + Top 3 = Flip play direction if forward or reverse
        // AUX + Top 4 = Increment play direction mode
        void onAUXFunc(uint8_t funcKey) override;

    private:
        OmniSeq seq_;

        uint8_t selStep_;

        void onEnabled();
        void onDisabled();

        void onEncoderChangedSelectParam(Encoder::Update enc);
        void onEncoderChangedEditParam(Encoder::Update enc);

        void onUIModeChanged(uint8_t prevMode, uint8_t newMode);

        Track *getTrack();

        void selStep(uint8_t stepIndex); // 0-15

        // char foo[sizeof(Track)]
    };
}