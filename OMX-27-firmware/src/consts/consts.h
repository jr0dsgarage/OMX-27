#pragma once

// OMX-27 shared constants

// HW_VERSIONS

#define PICO 1
#define OMX2040 2
#define TEENSY32 3
#define TEENSY4 4

#ifndef BOARDTYPE

// AUTOMATICALLY GET BOARD TYPE - DO NOT MODIFY
#ifdef ARDUINO_TEENSY40
// #define T4 1
	#define BOARDTYPE TEENSY4
#elif ARDUINO_TEENSY32
	#define BOARDTYPE TEENSY32
#else
// #define T4 0
	#define BOARDTYPE OMX2040
#endif

#endif

// #ifndef BOARDTYPE
// #define BOARDTYPE OMX2040
// #endif


#define DEV 0
#define MIDIONLY 0

#include "../hardware/hardware_config.h"

// Comment out defines to disable modes if needed for debug build
#define OMXMODEGRIDS

// HARDWARE Pin for CVGATE_PIN = 13 on beta1 boards, 22 on bodge/midi, 23 on 1.0
const int CVGATE_PIN = kCVGATE_PIN;
const int CVPITCH_PIN = kCVPITCH_PIN;

const int loSkip = 0;
const int hiSkip = 0;
constexpr int range = 4096 - loSkip - hiSkip;

const float fullRangeV = 4.66;
const float fullRangeDAC = 4095.0;
const float stepsPerVolt = fullRangeDAC / fullRangeV;
const float stepsPerOctave = stepsPerVolt;
const float stepsPerSemitone = stepsPerOctave / 12;

const uint8_t midiMiddleC = 60;
const uint8_t cvLowestNote = midiMiddleC - 3 * 12; // 3 is how many octaves under middle c
const uint8_t cvHightestNote = cvLowestNote + int(fullRangeV * 12) - 1;

// FONTS
#define FONT_LABELS u8g2_font_5x8_tf
#define FONT_VALUES u8g2_font_7x14B_tf
#define FONT_SYMB u8g2_font_9x15_m_symbols
#define FONT_SYMB_BIG u8g2_font_cu12_h_symbols
#define FONT_TENFAT u8g2_font_tenfatguys_tf
#define FONT_BIG u8g2_font_helvB18_tr
#define FONT_CHAR16 u8g2_font_6x12_tf
