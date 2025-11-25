#pragma once

#include "config.h"
#include "hardware/storage.h"
#include "midi/sysex.h"
#include "midi/MIDIClockStats.h"
#include "utils/music_scales.h"
#include "hardware/omx_keypad.h"
#include "ClearUI/ClearUI_Input.h"

class OmxModeMidiKeyboard;
class OmxModeDrum;
class OmxModeSequencer;
class OmxModeGrids;
class OmxModeEuclidean;
class OmxModeChords;
class OmxModeInterface;
class OmxScreensaver;

extern SysSettings sysSettings;
extern PotSettings potSettings;
extern MidiMacroConfig midiMacroConfig;
extern MIDIClockStats clockstats;

// setup EEPROM/FRAM storage
extern Storage *storage;
extern SysEx *sysEx;

extern OmxModeMidiKeyboard omxModeMidi;
extern OmxModeDrum omxModeDrum;
extern OmxModeSequencer omxModeSeq;
#ifdef OMXMODEGRIDS
extern OmxModeGrids omxModeGrids;
#endif
extern OmxModeEuclidean omxModeEuclid;
extern OmxModeChords omxModeChords;

extern OmxModeInterface *activeOmxMode;

extern OmxScreensaver omxScreensaver;

extern MusicScales globalScale;

extern int volatile currentValue[NUM_CC_POTS];
extern int lastMidiValue[NUM_CC_POTS];

extern Micros lastProcessTime;

extern uint8_t RES;
extern uint16_t AMAX;
extern int V_scale;

extern Encoder myEncoder;
extern const int buttonPin;
extern Button encButton;

extern OMXKeypad keypad;

#if BOARDTYPE == OMX2040
extern char mfgstr[32];
extern char prodstr[32];
extern const int muxMapping[5];
extern const int mux_common_pin;
extern const int mux1;
extern const int mux2;
extern const int mux3;
// Mux type needs Mux.h
#include <Mux.h>
using namespace admux;
extern Mux mux;

#include <Adafruit_TinyUSB.h>
extern Adafruit_USBD_WebUSB usb_web;
void setupWebUSB();
#endif

