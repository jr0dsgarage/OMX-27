#include "omx_hardware.h"
#include "hardware_config.h"
#include "../consts/consts.h"
#include "../globals.h"
#include <Wire.h>

void OmxHardware::setup() {
#if BOARDTYPE == OMX2040
    TinyUSBDevice.setManufacturerDescriptor(mfgstr);
    TinyUSBDevice.setProductDescriptor(prodstr);

    pinMode(kREDLED, OUTPUT);
    pinMode(kBLUELED, OUTPUT);
    digitalWrite(kREDLED, LOW);
    digitalWrite(kBLUELED, HIGH);

    pinMode(kFIVEVEN, OUTPUT);
    digitalWrite(kFIVEVEN, HIGH);

    pinMode(kTXLED, OUTPUT);
    pinMode(kRXLED, INPUT);

    digitalWrite(kTXLED, LOW);
    digitalWrite(kRXLED, LOW);
    Wire1.setSDA(kI2C_SDA);
    Wire1.setSCL(kI2C_SCL);
#endif
}

void OmxHardware::initDAC() {
#if BOARDTYPE == TEENSY4
    dac.begin(kDAC_ADDR);
#elif BOARDTYPE == OMX2040
    dac.begin(kDAC_ADDR, &Wire1);
#else
    // Teensy 3.2
    analogWriteResolution(12); // Hardcoded to match RES=12 in setup
#endif
}

void OmxHardware::initWebUSB() {
#if BOARDTYPE == OMX2040
    setupWebUSB();
#endif
}

void OmxHardware::initAnalog() {
#if BOARDTYPE == TEENSY4
    randomSeed(analogRead(13));
    srand(analogRead(13));
    analogReadResolution(10);
#elif BOARDTYPE == OMX2040
    // randomSeed(analogRead(29));
    // srand(analogRead(29));
    analogReadResolution(10);
#else
    randomSeed(analogRead(13));
    srand(analogRead(13));
    analogReadResolution(13);
#endif
}

void OmxHardware::initPots(PotSettings& potSettings) {
    for (int i = 0; i < potCount; i++) {
#if BOARDTYPE == TEENSY4
        pinMode(analogPins[i], INPUT);
        potSettings.analog[i] = new ResponsiveAnalogRead(analogPins[i], true, .001);
#elif BOARDTYPE == OMX2040
        potSettings.analog[i] = new ResponsiveAnalogRead(mux_common_pin, true, .001);
#else
        pinMode(analogPins[i], INPUT);
        potSettings.analog[i] = new ResponsiveAnalogRead(analogPins[i], true, .001);
        potSettings.analog[i]->setAnalogResolution(1 << 13);
        potSettings.analog[i]->setActivityThreshold(32);
#endif
        currentValue[i] = 0;
        lastMidiValue[i] = 0;
    }
}

void OmxHardware::setDAC(int voltage) {
#if BOARDTYPE == TEENSY4 || BOARDTYPE == OMX2040
    dac.setVoltage(voltage, false);
#else
    // Teensy 3.2 uses analogWrite for DAC
    // Assuming resolution is already set in setup or initDAC
    // But wait, analogWriteResolution(RES) was called in setup() for T3.2
    // I should probably move that here or to initDAC
    analogWrite(kCVPITCH_PIN, voltage);
#endif
}

int OmxHardware::readPot(int potIndex) {
#if BOARDTYPE == OMX2040
    return mux.read(muxMapping[potIndex]);
#else
    return analogRead(analogPins[potIndex]);
#endif
}

TwoWire& OmxHardware::getDisplayI2C() {
#if BOARDTYPE == OMX2040
    return Wire1;
#else
    return Wire;
#endif
}
