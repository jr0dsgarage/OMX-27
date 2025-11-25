#pragma once

#include <Arduino.h>
#include "../config.h" // For BOARDTYPE

// Hardware Pin Definitions and Board-Specific Constants

#if BOARDTYPE == OMX2040
    // --- OMX2040 (RP2040) Configuration ---
    
    // I2C
    const uint8_t kI2C_SDA = 2;
    const uint8_t kI2C_SCL = 3;

    // LEDs
    const uint8_t kTXLED = 0;
    const uint8_t kRXLED = 1;
    const int kREDLED = 16;
    const int kBLUELED = 18;
    const int kNEOPIXPIN = 19;
    const int kFIVEVEN = 17;
    const int kLED_PIN = 19;

    // CV/Gate
    const int kCVGATE_PIN = 27;
    // CVPITCH_PIN is not defined for OMX2040 in original code? 
    // Original code had empty #elif BOARDTYPE == OMX2040 for CVPITCH_PIN
    const int kCVPITCH_PIN = -1; // Placeholder

    // Analog / Pots
    const int kAnalogPins[] = {2, 3, 0, 1, 4}; // Mux pin numbers
    const byte kDAC_ADDR = 0x60;
    const int kPotMaxVal = 1018;
    const int kAnalogDeltaThreshold = 1;
    const int kGridsDeltaThreshold = 1;

    // Key Matrix
    const byte kRowPins[] = {28, 14, 13, 12, 6};
    const byte kColPins[] = {10, 9, 4, 5, 8, 11};

    // Encoder
    const int kEncoderPin1 = 25;
    const int kEncoderPin2 = 26;
    const int kButtonPin = 20;

#elif BOARDTYPE == TEENSY4
    // --- Teensy 4.0 Configuration ---

    // LEDs
    const int kLED_PIN = 14;

    // CV/Gate
    const int kCVGATE_PIN = 13;
    // CVPITCH_PIN commented out in original?
    const int kCVPITCH_PIN = A14; // Assuming A14 based on #else block

    // Analog / Pots
    const int kAnalogPins[] = {23, 22, 21, 20, 16};
    const byte kDAC_ADDR = 0x60;
    const int kPotMaxVal = 1019;
    const int kAnalogDeltaThreshold = 1;
    const int kGridsDeltaThreshold = 1;

    // Key Matrix
    const byte kRowPins[] = {6, 4, 3, 5, 2};
    const byte kColPins[] = {7, 8, 10, 9, 15, 17};

    // Encoder
    const int kEncoderPin1 = 12;
    const int kEncoderPin2 = 11;
    const int kButtonPin = 0;

#else 
    // --- Teensy 3.2 / Default Configuration ---

    // LEDs
    const int kLED_PIN = 14;

    // CV/Gate
    #if DEV
        const int kCVGATE_PIN = 13;
    #elif MIDIONLY
        const int kCVGATE_PIN = 22;
    #else
        const int kCVGATE_PIN = 23;
    #endif
    const int kCVPITCH_PIN = A14;

    // Analog / Pots
    #if DEV
        const int kAnalogPins[] = {23, 22, 21, 20, 16};
        const byte kDAC_ADDR = 0x62;
    #elif MIDIONLY
        const int kAnalogPins[] = {23, 22, 21, 20, 16}; // A10 broken on test/midi
        const byte kDAC_ADDR = 0x60;
    #else
        const int kAnalogPins[] = {34, 22, 21, 20, 16};
        const byte kDAC_ADDR = 0x60;
    #endif

    const int kPotMaxVal = 8191;
    const int kAnalogDeltaThreshold = 3;
    const int kGridsDeltaThreshold = 6;

    // Key Matrix
    const byte kRowPins[] = {6, 4, 3, 5, 2};
    const byte kColPins[] = {7, 8, 10, 9, 15, 17};

    // Encoder
    const int kEncoderPin1 = 12;
    const int kEncoderPin2 = 11;
    const int kButtonPin = 0;

#endif

// Common Constants
const int kLedCount = 27;
