#pragma once
#include "../../ClearUI/ClearUI_Input.h"
#include "../../hardware/omx_keypad.h"
#include "../../utils/param_manager.h"

// defines interface for a submode, a mode within a mode
class FormMachineInterface
{
public:
	FormMachineInterface() {}
	virtual ~FormMachineInterface() {}

    // Getters
	virtual bool isEnabled();
	virtual bool usesPots() { return false; } // return true if submode uses pots
	virtual bool shouldBlockEncEdit() { return false; }
	virtual bool getEncoderSelect();

    // Setters
	virtual void setEnabled(bool newEnabled);

    // Standard Updates
	virtual void onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta) {}
	virtual void onClockTick() {}
	virtual void loopUpdate() {}
	virtual bool updateLEDs() { return true; }
	virtual void onEncoderChanged(Encoder::Update enc);
	virtual void onEncoderButtonDown();
	virtual bool onKeyUpdate(OMXKeypadEvent e) { return true; }
	virtual bool onKeyHeldUpdate(OMXKeypadEvent e) { return true; }

	virtual void onDisplayUpdate() {}


protected:
	bool enabled_;
	bool encoderSelect_;

	virtual void onEnabled() {}	 // Called whenever entering mode
	virtual void onDisabled() {} // Called whenever exiting mode

	virtual void onEncoderChangedSelectParam(Encoder::Update enc);
	virtual void onEncoderChangedEditParam(Encoder::Update enc) = 0;
};
