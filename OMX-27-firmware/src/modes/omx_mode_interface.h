#pragma once
#include "../ClearUI/ClearUI_Input.h"
#include "../hardware/omx_keypad.h"
#include "../config.h"

/**
 * @class OmxModeInterface
 * @brief Interface class for all omxModes.
 *
 * This class is used to define the methods that an omxMode should implement.
 *
 */
class OmxModeInterface
{
public:
	// Class Constructer
	OmxModeInterface() {}
	// Class Destructor
	virtual ~OmxModeInterface() {}

	/**
	 * @brief Called upon initialization of the omxMode
	 */
	virtual void InitSetup() {}

	/**
	 * @brief Called whenever entering the omxMode
	 */
	virtual void onModeActivated() {}

	/**
	 * @brief Called whenever leaving the omxMode
	 */
	virtual void onModeDeactivated() {}

	/**
	 * @brief Defines what happens during a clock tick while using the omxMode
	 */
	virtual void onClockTick() {}

	/**
	 * @brief Called whenever a potentiometer is adjusted while using the omxMode
	 *
	 * @param potIndex The index of the potentiometer
	 * @param prevValue The previous value of the potentiometer
	 * @param newValue The new value of the potentiometer
	 * @param analogDelta The analog delta between the previous and new value
	 */
	virtual void onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta) = 0;

	/**
	 * @brief Defines what happens during the loop update
	 *
	 * @param elapsedTime The time elapsed since the last loop update
	 */
	virtual void loopUpdate(Micros elapsedTime) {}

	/**
	 * @brief Defines how the LEDs are updated while the omxMode is active
	 */
	virtual void updateLEDs() = 0;

	/**
	 * @brief Defines what happens when the encoder is adjusted while using the omxMode
	 *
	 * @param enc The ClearUI encoder object that is being adjusted
	 */
	virtual void onEncoderChanged(Encoder::Update enc) = 0;

	/**
	 * @brief Defines what happens when the encoder button is pressed while using the omxMode
	 */
	virtual void onEncoderButtonDown() = 0;

	/**
	 * @brief Defines what happens when the encoder button is released while using the omxMode
	 */
	virtual void onEncoderButtonUp(){};

	/**
	 * @brief Defines what happens when the encoder button is released after a long press while using the omxMode
	 */
	virtual void onEncoderButtonUpLong(){};

	/**
	 * @brief Returns true if the encoder button should block encoder mode switch / hold down encoder
	 *
	 * @return true
	 * @return false
	 */
	virtual bool shouldBlockEncEdit() { return false; }

	/**
	 * @brief Defines what happens after a long press of the encoder button while using the omxMode
	 * @brief if the Encoder should be blocked from going into edit mode on a long press
	 *
	 * @note Will only get called if shouldBlockEncEdit() returns true
	 */
	virtual void onEncoderButtonDownLong() = 0;			// Will only get called if shouldBlockEncEdit() returns true

	/**
	 * @brief Defines what happens when a keypad button is pressed while using the omxMode
	 *
	 * @param e The OMXKeypadEvent object from the key that is being pressed
	 */
	virtual void onKeyUpdate(OMXKeypadEvent e) = 0;

	/**
	 * @brief Defines what happens when a keypad button is held down while using the omxMode
	 *
	 * @param e The OMXKeypadEvent object from the key that is being held down
	 */
	virtual void onKeyHeldUpdate(OMXKeypadEvent e){};

	/**
	 * @brief Defines what to do when the display should be updated
	 *
	 */
	virtual void onDisplayUpdate(){};

	// #### Inbound MIDI callbacks

	/**
	 * @brief Defines what happens when a MIDI note is played while using the omxMode
	 *
	 * @param channel The MIDI channel the note is played on
	 * @param note The note that is played
	 * @param velocity The velocity of the note
	 */
	virtual void inMidiNoteOn(byte channel, byte note, byte velocity) {}

	/**
	 * @brief Defines what happens when a MIDI note is released while using the omxMode
	 *
	 * @param channel The MIDI channel the note is played on
	 * @param note The note that is released
	 * @param velocity The velocity of the note
	 */
	virtual void inMidiNoteOff(byte channel, byte note, byte velocity) {}

	/**
	 * @brief Defines what happens when a MIDI control change is made while using the omxMode
	 *
	 * @param channel The MIDI channel the control change is made on
	 * @param control The control that is changed
	 * @param value The new value of the control
	 */
	virtual void inMidiControlChange(byte channel, byte control, byte value) {}
};
