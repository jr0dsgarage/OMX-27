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

        void setTest() override;

	    void playBackStateChanged(bool newIsPlaying) override;
	    void resetPlayback() override;

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

        void changeUIMode(uint8_t newMode, bool silent);
        void onUIModeChanged(uint8_t prevMode, uint8_t newMode);

        Track *getTrack();
        Step *getSelStep();

        void selStep(uint8_t stepIndex); // 0-15

        uint8_t playingStep_;

        uint8_t lastTriggeredStepIndex;

        Micros nextStepTime_;

        Micros stepMicros_;

        uint16_t ticksPerStep_;

        uint16_t omniTick_;

        int16_t ticksTilNextTrigger_;

        int16_t ticksTilNext16Trigger_; // Keeps track of ticks to quantized next 16th

        int16_t ticksTilNextTriggerRate_;

        float stepLengthMult_ = 1.0f; // 1 is a 16th note, 0.5 a 32nd note length, recalculated with the rate

        std::vector<MidiNoteGroup> triggeredNotes_;

        std::vector<MidiNoteGroup> noteOns_;

        void onRateChanged();

        float getStepLenMult(uint8_t len);
        float getGateMult(uint8_t gate);

        MidiNoteGroup step2NoteGroup(uint8_t noteIndex, Step *step);
        void triggerStep(Step *step);

        // char foo[sizeof(Track)]
    };
}