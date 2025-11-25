#include "omx_mode_mgr.h"
#include "../globals.h"
#include "../hardware/omx_leds.h"
#include "../hardware/omx_disp.h"
#include "omx_mode_midi_keyboard.h"
#include "omx_mode_drum.h"
#include "omx_mode_sequencer.h"
#include "omx_mode_grids.h"
#include "omx_mode_euclidean.h"
#include "omx_mode_chords.h"

void OmxModeMgr::changeOmxMode(OMXMode newOmxmode)
{
	//	Serial.println((String)"NewMode: " + newOmxmode);
	sysSettings.omxMode = newOmxmode;
	sysSettings.newmode = newOmxmode;

	if (activeOmxMode != nullptr)
	{
		activeOmxMode->onModeDeactivated();
	}

	switch (newOmxmode)
	{
	case MODE_MIDI:
		omxModeMidi.setMidiMode();
		activeOmxMode = &omxModeMidi;
		break;
	case MODE_DRUM:
		activeOmxMode = &omxModeDrum;
		break;
	case MODE_CHORDS:
		activeOmxMode = &omxModeChords;
		break;
	case MODE_S1:
		omxModeSeq.setSeq1Mode();
		activeOmxMode = &omxModeSeq;
		break;
	case MODE_S2:
		omxModeSeq.setSeq2Mode();
		activeOmxMode = &omxModeSeq;
		break;
	case MODE_OM:
		omxModeMidi.setOrganelleMode();
		activeOmxMode = &omxModeMidi;
		break;
	case MODE_GRIDS:
#ifdef OMXMODEGRIDS
		activeOmxMode = &omxModeGrids;
#endif
		break;
	case MODE_EUCLID:
		activeOmxMode = &omxModeEuclid;
		break;
	default:
		omxModeMidi.setMidiMode();
		activeOmxMode = &omxModeMidi;
		break;
	}

	activeOmxMode->onModeActivated();

	omxLeds.setDirty();
	omxDisp.setDirty();
}
