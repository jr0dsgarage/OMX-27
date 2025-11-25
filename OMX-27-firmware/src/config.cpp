#include "config.h"
#include "consts/consts.h"
#include "hardware/hardware_config.h"

const OMXMode DEFAULT_MODE = MODE_MIDI;
const uint8_t EEPROM_VERSION = 38;

// v30 - adds storage to header for velocity
// v31 - adds storage for drums
// v32 - adds mfx chord saves
// v33 - adds mfx selector saves
// v34 - adds mfx repeat saves
// v35 - adds quantize rate to arps, added global quant rate to header
// v36 - adds stuff to randomizer

// DEFINE CC NUMBERS FOR POTS // CCS mapped to Organelle Defaults
const int CC1 = 21;
const int CC2 = 22;
const int CC3 = 23;
const int CC4 = 24;
const int CC5 = 7; // change to 25 for EYESY Knob 5

const int CC_AUX = 25; // Mother mode - AUX key
const int CC_OM1 = 26; // Mother mode - enc switch
const int CC_OM2 = 28; // Mother mode - enc turn

const int LED_BRIGHTNESS = 90;

// DONT CHANGE ANYTHING BELOW HERE
const int LED_COUNT = kLedCount;
const int LED_PIN = kLED_PIN;

const int analogPins[] = {kAnalogPins[0], kAnalogPins[1], kAnalogPins[2], kAnalogPins[3], kAnalogPins[4]};
const byte DAC_ADDR = kDAC_ADDR;

const int potCount = NUM_CC_POTS;

int pots[NUM_CC_BANKS][NUM_CC_POTS] = {
	{CC1, CC2, CC3, CC4, CC5},
	{29, 30, 31, 32, 33},
	{34, 35, 36, 37, 38},
	{39, 40, 41, 42, 43},
	{91, 93, 103, 104, 7}}; // the MIDI CC (continuous controller) for each analog input

int potMinVal = 0;
int potMaxVal = kPotMaxVal;

const int gridh = 32;
const int gridw = 128;
const int PPQ = 96; // Pulses Per Quarter note

const uint32_t secs2micros = 1000000;


const char *mfxOffMsg = "MidiFX are Off";
const char *mfxArpEditMsg = "Arp Edit";
const char *mfxPassthroughEditMsg = "MFX Quickedit";
const char *exitMsg = "Exit";
const char *paramOffMsg = "OFF";
const char *paramOnMsg = "ON";

const char *modes[] = {"MI", "DRUM", "CH", "S1", "S2", "GR", "EL", "OM"};
const char *macromodes[] = {"Off", "M8", "NRN", "DEL"};
const int nummacromodes = 3;

float multValues[] = {.25, .5, 1, 2, 4, 8, 16};
const char *mdivs[] = {"1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "W"};

const float kNoteLengths[] = {0.10, 0.25, 0.5, 0.75, 1, 1.5, 2, 4, 8, 16};
const uint8_t kNumNoteLengths = 10;

const uint8_t kArpRates[] = {1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64};
const uint8_t kNumArpRates = 16;

String tempString = "12345";
String tempStrings[8] = {"12345", "12345", "12345", "12345", "12345", "12345", "12345", "12345"};

// KEY SWITCH ROWS/COLS

// Map the keys
char keys[ROWS][COLS] = {
	{0, 1, 2, 3, 4, 5},
	{6, 7, 8, 9, 10, 26},
	{11, 12, 13, 14, 15, 24},
	{16, 17, 18, 19, 20, 25},
	{22, 23, 21}};

byte rowPins[ROWS] = {kRowPins[0], kRowPins[1], kRowPins[2], kRowPins[3], kRowPins[4]};
byte colPins[COLS] = {kColPins[0], kColPins[1], kColPins[2], kColPins[3], kColPins[4], kColPins[5]};

// KEYBOARD MIDI NOTE LAYOUT
const int notes[] = {0,
					 61, 63, 66, 68, 70, 73, 75, 78, 80, 82,
					 59, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84};

const int steps[] = {0,
					 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
					 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};

const int midiKeyMap[] = {12, 1, 13, 2, 14, 15, 3, 16, 4, 17, 5, 18, 19, 6, 20, 7, 21, 22, 8, 23, 9, 24, 10, 25, 26};

Adafruit_MCP4725 dac;
MidiConfig midiSettings;
EncoderConfig encoderConfig;
ClockConfig clockConfig;
SequencerConfig seqConfig;
ColorConfig colorConfig;
ScaleConfig scaleConfig;

// MidiPage midiPageParams;
// SequencerPage seqPageParams;
