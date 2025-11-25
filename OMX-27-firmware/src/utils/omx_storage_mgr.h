#pragma once
#include "../config.h"

class OmxStorageMgr {
public:
    static void saveHeader();
    static bool loadHeader();
    static void savePatterns();
    static void loadPatterns();
    static void saveToStorage();
    static bool loadFromStorage();
};
