// OMX-27 MIDI KEYBOARD / SEQUENCER

//	v1.14.0
//	Last update: Sept 2024
//
//	Original concept and initial code by Steven Noreyko
//  Additional code contributions:
// 		Matt Boone, Steven Zydek,
// 		Chris Atkins, Will Winder,
// 		Michael P Jones
//
//	Big thanks to:
//	John Park and Gerald Stevens for initial testing and feature ideas
//	mzero for immense amounts of code coaching/assistance
//	drjohn for support
//

#include "src/omx_main.h"
#include "src/consts/consts.h"

// Allows code to compile with smallest code LTO

#if BOARDTYPE != OMX2040
extern "C"
{
	int _getpid() { return -1; }
	int _kill(int pid, int sig) { return -1; }
	int _write() { return -1; }
}
#endif

OMXMain omxMain;

void setup()
{
	omxMain.setup();
}

void loop()
{
	omxMain.loop();
}
