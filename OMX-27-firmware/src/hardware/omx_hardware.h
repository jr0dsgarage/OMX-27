#pragma once

#include "../config.h"
#include "../globals.h"

class OmxHardware {
public:
    static void setup();
    static void initDAC();
    static void initWebUSB();
    static void initAnalog();
    static void initPots(PotSettings& potSettings);
    static void setDAC(int voltage);
    static int readPot(int potIndex);
    static TwoWire& getDisplayI2C();
};
