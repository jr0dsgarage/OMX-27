#pragma once

#include "../modes/omx_mode_interface.h"
#include "../utils/music_scales.h"
#include "../utils/param_manager.h"
#include "../modes/submodes/submode_midifxgroup.h"
#include "../modes/submodes/submode_potconfig.h"
#include "../modes/submodes/submode_preset.h"
#include "../midifx/midifx_interface.h"
#include "../midifx/midifx_interface.h"
#include "../midimacro/midimacro_m8.h"
#include "../midimacro/midimacro_norns.h"
#include "../midimacro/midimacro_deluge.h"


// Make sequencer keys light up as notes are triggered


// This mode is designed to be used with samplers or drum machines
// Each key can be configured to whatever Note, Vel, Midi Chan you want. 
// This class is very similar to the midi keyboard, maybe we merge or inherit. 
class OmxModeForm : public OmxModeInterface
{
public:
	OmxModeForm();
	~OmxModeForm() {}

	void InitSetup() override;
	void onModeActivated() override;
	void onModeDeactivated() override;

	void onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta) override;
	void loopUpdate(Micros elapsedTime) override;
	void onClockTick() override;

	void updateLEDs() override;

	void onEncoderChanged(Encoder::Update enc) override;
	void onEncoderButtonDown() override;
	void onEncoderButtonUp() override;

	void onEncoderButtonDownLong() override;

	bool shouldBlockEncEdit() override;

	void onKeyUpdate(OMXKeypadEvent e) override;
	void onKeyHeldUpdate(OMXKeypadEvent e) override;

	void onDisplayUpdate() override;
	void inMidiNoteOn(byte channel, byte note, byte velocity) override;
	void inMidiNoteOff(byte channel, byte note, byte velocity) override;
	void inMidiControlChange(byte channel, byte control, byte value) override;

	void SetScale(MusicScales *scale);

	int saveToDisk(int startingAddress, Storage *storage);
	int loadFromDisk(int startingAddress, Storage *storage);
private:
	SubModePreset presetManager;
	MusicScales *musicScale;

	bool initSetup = false;

	// If true, encoder selects param rather than modifies value
	bool encoderSelect = false;
	// void onEncoderChangedSelectParam(Encoder::Update enc);
	ParamManager params;

	bool macroActive_ = false;
	bool mfxQuickEdit_ = false;
	uint8_t quickEditMfxIndex_ = 0;

	bool getEncoderSelect();

	// SubModes
	SubmodeInterface *activeSubmode = nullptr;
	SubModePotConfig subModePotConfig_;

	void enableSubmode(SubmodeInterface *subMode);
	void disableSubmode();
	bool isSubmodeEnabled();

    bool onKeyUpdateSelMidiFX(OMXKeypadEvent e);
	bool onKeyHeldSelMidiFX(OMXKeypadEvent e);

	void doNoteOn(uint8_t keyIndex);
	void doNoteOff(uint8_t keyIndex);

	// Static glue to link a pointer to a member function
	static void onNotePostFXForwarder(void *context, MidiNoteGroup note)
	{
		static_cast<OmxModeForm *>(context)->onNotePostFX(note);
	}

	void onNotePostFX(MidiNoteGroup note);

	// Static glue to link a pointer to a member function
	static void onPendingNoteOffForwarder(void *context, int note, int channel)
	{
		static_cast<OmxModeForm *>(context)->onPendingNoteOff(note, channel);
	}

	void onPendingNoteOff(int note, int channel);

	void stopSequencers();

	void selectMidiFx(uint8_t mfxIndex, bool dispMsg);

	// uint8_t mfxIndex_ = 0;

	midimacro::MidiMacroNorns nornsMarco_;
	midimacro::MidiMacroM8 m8Macro_;
	midimacro::MidiMacroDeluge delugeMacro_;

	midimacro::MidiMacroInterface *activeMacro_;

	midimacro::MidiMacroInterface *getActiveMacro();

	void saveKit(uint8_t saveIndex);
	void loadKit(uint8_t loadIndex);

	static void doSaveKitForwarder(void *context, uint8_t kitIndex)
	{
		static_cast<OmxModeForm *>(context)->saveKit(kitIndex);
	}

	static void doLoadKitForwarder(void *context, uint8_t kitIndex)
	{
		static_cast<OmxModeForm *>(context)->loadKit(kitIndex);
	}

	// Static glue to link a pointer to a member function
	static void doNoteOnForwarder(void *context, uint8_t keyIndex)
	{
		static_cast<OmxModeForm *>(context)->doNoteOn(keyIndex);
	}

	// Static glue to link a pointer to a member function
	static void doNoteOffForwarder(void *context, uint8_t keyIndex)
	{
		static_cast<OmxModeForm *>(context)->doNoteOff(keyIndex);
	}
};
