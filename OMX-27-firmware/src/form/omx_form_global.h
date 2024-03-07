#pragma once
#include "../ClearUI/ClearUI_Input.h"
// #include "../../hardware/omx_keypad.h"
// #include "../../utils/param_manager.h"
#include "../utils/music_scales.h"

enum ShortCutMode
{
	FORMSHORTCUT_NONE, // No shortcut keys
	FORMSHORTCUT_AUX,  // Aux shortcut held
	FORMSHORTCUT_F1,   // Top key 1 held
	FORMSHORTCUT_F2,   // Top key 2 held
	FORMSHORTCUT_F3	   // Top key 1 & 2 held
};

enum FormMode
{
	FORMMODE_BASE,
	FORMMODE_SELECTMACHINE,
	FORMMODE_COUNT
};

// Singleton class that form machines and base form mode can use to stay in sync
class OmxFormGlobalSettings
{
public:
bool encoderSelect = false;
MusicScales *musicScale;

uint8_t shortcutMode;
uint8_t formMode;
uint8_t selMidiFX;
};

extern OmxFormGlobalSettings omxFormGlobal;