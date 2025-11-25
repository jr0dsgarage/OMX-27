#include "globals.h"
#include "hardware/hardware_config.h"
#include "modes/omx_mode_midi_keyboard.h"
#include "modes/omx_mode_drum.h"
#include "modes/omx_mode_sequencer.h"
#include "modes/omx_mode_grids.h"
#include "modes/omx_mode_euclidean.h"
#include "modes/omx_mode_chords.h"
#include "modes/omx_screensaver.h"

SysSettings sysSettings;
PotSettings potSettings;
MidiMacroConfig midiMacroConfig;

// setup EEPROM/FRAM storage
Storage *storage;
SysEx *sysEx;

OmxModeMidiKeyboard omxModeMidi;
OmxModeDrum omxModeDrum;
OmxModeSequencer omxModeSeq;
#ifdef OMXMODEGRIDS
OmxModeGrids omxModeGrids;
#endif
OmxModeEuclidean omxModeEuclid;
OmxModeChords omxModeChords;

OmxModeInterface *activeOmxMode;

OmxScreensaver omxScreensaver;

MusicScales globalScale;

MIDIClockStats clockstats;

int volatile currentValue[NUM_CC_POTS];
int lastMidiValue[NUM_CC_POTS];

Micros lastProcessTime;

uint8_t RES;
uint16_t AMAX;
int V_scale;

// ENCODER
Encoder myEncoder(kEncoderPin1, kEncoderPin2); // encoder pins on hardware
const int buttonPin = kButtonPin;
Button encButton(buttonPin);

// KEYPAD
unsigned long longPressInterval = 800;
unsigned long clickWindow = 200;
OMXKeypad keypad(longPressInterval, clickWindow, makeKeymap(keys), rowPins, colPins, ROWS, COLS);

#if BOARDTYPE == OMX2040
	char mfgstr[32] = "denki-oto";
	char prodstr[32] = "omx-27-v3";
	// MUX config
	const int muxMapping[5] = {2,3,0,1,4}; //{A2, A3, A0, A1, A4};
	const int mux_common_pin = 29;
	const int mux1 = 23;
	const int mux2 = 24;
	const int mux3 = 22;
	using namespace admux;
	Mux mux(Pin(mux_common_pin, INPUT, PinType::Analog), Pinset(mux1, mux2, mux3));

	// USB WebUSB object
	// Landing Page: scheme (0: http, 1: https), url
	// Page source can be found at https://github.com/hathach/tinyusb-webusb-page/tree/main/webusb-rgb
	Adafruit_USBD_WebUSB usb_web;
	WEBUSB_URL_DEF(landingPage, 1 /*https*/, "denki-oto-to-go-go.surge.sh/#/");

    void setupWebUSB() {
        usb_web.setLandingPage(&landingPage);
        usb_web.begin();
    }

#endif

