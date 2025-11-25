#include "omx_storage_mgr.h"
#include "../globals.h"
#include "../modes/omx_mode_mgr.h"
#include "../hardware/omx_disp.h"
#include "../hardware/omx_leds.h"
#include "../modes/sequencer.h"
#include "../modes/submodes/submode_midifxgroup.h"
#include "../utils/cvNote_util.h"
#include "../modes/omx_mode_midi_keyboard.h"
#include "../modes/omx_mode_drum.h"
#include "../modes/omx_mode_sequencer.h"
#include "../modes/omx_mode_grids.h"
#include "../modes/omx_mode_euclidean.h"
#include "../modes/omx_mode_chords.h"

void OmxStorageMgr::saveHeader()
{
	// 1 byte for EEPROM version
	storage->write(EEPROM_HEADER_ADDRESS + 0, EEPROM_VERSION);

	// 1 byte for mode
	storage->write(EEPROM_HEADER_ADDRESS + 1, (uint8_t)sysSettings.omxMode);

	// 1 byte for the active pattern
	storage->write(EEPROM_HEADER_ADDRESS + 2, (uint8_t)sequencer.playingPattern);

	// 1 byte for Midi channel
	uint8_t unMidiChannel = (uint8_t)(sysSettings.midiChannel - 1);
	storage->write(EEPROM_HEADER_ADDRESS + 3, unMidiChannel);

	for (int b = 0; b < NUM_CC_BANKS; b++)
	{
		for (int i = 0; i < NUM_CC_POTS; i++)
		{
			storage->write(EEPROM_HEADER_ADDRESS + 4 + i + (5 * b), pots[b][i]);
		}
	}
	// Last is 28

	uint8_t midiMacroChan = (uint8_t)(midiMacroConfig.midiMacroChan - 1);
	storage->write(EEPROM_HEADER_ADDRESS + 29, midiMacroChan);

	uint8_t midiMacroId = (uint8_t)midiMacroConfig.midiMacro;
	storage->write(EEPROM_HEADER_ADDRESS + 30, midiMacroId);

	uint8_t scaleRoot = (uint8_t)scaleConfig.scaleRoot;
	storage->write(EEPROM_HEADER_ADDRESS + 31, scaleRoot);

	uint8_t scalePattern = (uint8_t)scaleConfig.scalePattern;
	storage->write(EEPROM_HEADER_ADDRESS + 32, scalePattern);

	uint8_t lockScale = (uint8_t)scaleConfig.lockScale;
	storage->write(EEPROM_HEADER_ADDRESS + 33, lockScale);

	uint8_t scaleGrp16 = (uint8_t)scaleConfig.group16;
	storage->write(EEPROM_HEADER_ADDRESS + 34, scaleGrp16);

	storage->write(EEPROM_HEADER_ADDRESS + 35, midiSettings.defaultVelocity);

	storage->write(EEPROM_HEADER_ADDRESS + 36, clockConfig.globalQuantizeStepIndex);

	storage->write(EEPROM_HEADER_ADDRESS + 37, cvNoteUtil.triggerMode);

	storage->write(EEPROM_HEADER_ADDRESS + 38, potSettings.potbank);

	// 38 bytes
}

bool OmxStorageMgr::loadHeader(void)
{
	uint8_t version = storage->read(EEPROM_HEADER_ADDRESS + 0);

	char buf[64];
	snprintf(buf, sizeof(buf), "EEPROM Header Version is %d\n", version);
	// Serial.print(buf);

	// Uninitalized EEPROM memory is filled with 0xFF
	if (version == 0xFF)
	{
		// EEPROM was uninitialized
		// Serial.println("version was 0xFF");
		return false;
	}

	if (version != EEPROM_VERSION)
	{
		// write an adapter if we ever need to increment the EEPROM version and also save the existing patterns
		// for now, return false will essentially reset the state
		// Serial.println("version not matched");
		return false;
	}

	sysSettings.omxMode = (OMXMode)storage->read(EEPROM_HEADER_ADDRESS + 1);

	sequencer.playingPattern = storage->read(EEPROM_HEADER_ADDRESS + 2);
	sysSettings.playingPattern = sequencer.playingPattern;

	uint8_t unMidiChannel = storage->read(EEPROM_HEADER_ADDRESS + 3);
	sysSettings.midiChannel = unMidiChannel + 1;

	// Serial.println("Loading banks");
	for (int b = 0; b < NUM_CC_BANKS; b++)
	{
		for (int i = 0; i < NUM_CC_POTS; i++)
		{
			pots[b][i] = storage->read(EEPROM_HEADER_ADDRESS + 4 + i + (5 * b));
		}
	}

	uint8_t midiMacroChannel = storage->read(EEPROM_HEADER_ADDRESS + 29);
	midiMacroConfig.midiMacroChan = midiMacroChannel + 1;

	uint8_t midiMacro = storage->read(EEPROM_HEADER_ADDRESS + 30);
	midiMacroConfig.midiMacro = midiMacro;

	uint8_t scaleRoot = storage->read(EEPROM_HEADER_ADDRESS + 31);
	scaleConfig.scaleRoot = scaleRoot;

	int8_t scalePattern = (int8_t)storage->read(EEPROM_HEADER_ADDRESS + 32);
	scaleConfig.scalePattern = scalePattern;

	bool lockScale = (bool)storage->read(EEPROM_HEADER_ADDRESS + 33);
	scaleConfig.lockScale = lockScale;

	bool scaleGrp16 = (bool)storage->read(EEPROM_HEADER_ADDRESS + 34);
	scaleConfig.group16 = scaleGrp16;

	globalScale.calculateScale(scaleConfig.scaleRoot, scaleConfig.scalePattern);

	midiSettings.defaultVelocity = storage->read(EEPROM_HEADER_ADDRESS + 35);

	clockConfig.globalQuantizeStepIndex = constrain(storage->read(EEPROM_HEADER_ADDRESS + 36), 0, kNumArpRates - 1);

	cvNoteUtil.triggerMode = constrain(storage->read(EEPROM_HEADER_ADDRESS + 37), 0, 1);

	potSettings.potbank = constrain(storage->read(EEPROM_HEADER_ADDRESS + 38), 0, NUM_CC_BANKS-1);

	// digitalWrite(BLUELED, HIGH);
	return true;
}

void OmxStorageMgr::savePatterns(void)
{
	bool isEeprom = storage->isEeprom();

	int patternSize = serializedPatternSize(isEeprom);
	int nLocalAddress = EEPROM_PATTERN_ADDRESS;

	// Serial.println((String)"Seq patternSize: " + patternSize);
	int seqPatternNum = isEeprom ? NUM_SEQ_PATTERNS_EEPROM : NUM_SEQ_PATTERNS;

	for (int i = 0; i < seqPatternNum; i++)
	{
		auto pattern = (byte *)sequencer.getPattern(i);
		for (int j = 0; j < patternSize; j++)
		{
			storage->write(nLocalAddress + j, *pattern++);
		}

		nLocalAddress += patternSize;
	}

	if (isEeprom)
	{
		return;
	}
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 5784

#ifdef OMXMODEGRIDS
	// Serial.println("Saving Grids");

	// Grids patterns
	patternSize = OmxModeGrids::serializedPatternSize(isEeprom);
	int numPatterns = OmxModeGrids::getNumPatterns();

	// Serial.println((String)"OmxModeGrids patternSize: " + patternSize);
	// Serial.println((String)"numPatterns: " + numPatterns);

	for (int i = 0; i < numPatterns; i++)
	{
		auto pattern = (byte *)omxModeGrids.getPattern(i);
		for (int j = 0; j < patternSize; j++)
		{
			storage->write(nLocalAddress + j, *pattern++);
		}

		nLocalAddress += patternSize;
	}
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 6008
#endif

	// Serial.println("Saving Euclidean");
	nLocalAddress = omxModeEuclid.saveToDisk(nLocalAddress, storage);
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 7433

	// Serial.println("Saving Chords");
	nLocalAddress = omxModeChords.saveToDisk(nLocalAddress, storage);
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 10505

	// Serial.println("Saving Drums");
	nLocalAddress = omxModeDrum.saveToDisk(nLocalAddress, storage);
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 11545

	// Serial.println("Saving MidiFX");
	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		nLocalAddress = subModeMidiFx[i].saveToDisk(nLocalAddress, storage);
		// Serial.println((String)"Saved: " + i);
		// Serial.println((String)"nLocalAddress: " + nLocalAddress);
	}
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 11585
}

void OmxStorageMgr::loadPatterns(void)
{
	bool isEeprom = storage->isEeprom();

	int patternSize = serializedPatternSize(isEeprom);
	int nLocalAddress = EEPROM_PATTERN_ADDRESS;

	// Serial.print("Seq patterns - nLocalAddress: ");
	// Serial.println(nLocalAddress);

	int seqPatternNum = isEeprom ? NUM_SEQ_PATTERNS_EEPROM : NUM_SEQ_PATTERNS;

	for (int i = 0; i < seqPatternNum; i++)
	{
		auto pattern = (byte *)sequencer.getPattern(i);
		auto current = (byte *)pattern;
		for (int j = 0; j < patternSize; j++)
		{
			*current = storage->read(nLocalAddress + j);
			current++;
		}
		// sequencer.patterns[i] = pattern; // getPattern returns pointer, so we wrote directly to it.

		nLocalAddress += patternSize;
	}

	if (isEeprom)
	{
		return;
	}

	// Serial.print("Grids patterns - nLocalAddress: ");
	// Serial.println(nLocalAddress);
	// 332 - eeprom size
	// 332 * 8 = 2656

	// Grids patterns
#ifdef OMXMODEGRIDS
	patternSize = OmxModeGrids::serializedPatternSize(isEeprom);
	int numPatterns = OmxModeGrids::getNumPatterns();

	for (int i = 0; i < numPatterns; i++)
	{
		auto pattern = (byte *)omxModeGrids.getPattern(i);
		auto current = (byte *)pattern;
		for (int j = 0; j < patternSize; j++)
		{
			*current = storage->read(nLocalAddress + j);
			current++;
		}
		// omxModeGrids.setPattern(i, pattern); // getPattern returns pointer
		nLocalAddress += patternSize;
	}
#endif

	// Serial.print("Pattern size: ");
	// Serial.print(patternSize);

	// Serial.print(" - nLocalAddress: ");
	// Serial.println(nLocalAddress);

	// Serial.print("Loading Euclidean - ");
	nLocalAddress = omxModeEuclid.loadFromDisk(nLocalAddress, storage);
	// Serial.println((String) "nLocalAddress: " + nLocalAddress); // 5988

	// Serial.print("Loading Chords - ");
	nLocalAddress = omxModeChords.loadFromDisk(nLocalAddress, storage);
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 5988

	// Serial.print("Loading Drums - ");
	nLocalAddress = omxModeDrum.loadFromDisk(nLocalAddress, storage);
	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 5988

	// Serial.println((String)"nLocalAddress: " + nLocalAddress); // 5968

	// Serial.print("Loading MidiFX - ");
	for (uint8_t i = 0; i < NUM_MIDIFX_GROUPS; i++)
	{
		nLocalAddress = subModeMidiFx[i].loadFromDisk(nLocalAddress, storage);
		// Serial.println((String)"Loaded: " + i);
		// Serial.println((String)"nLocalAddress: " + nLocalAddress);
	}
	// Serial.println((String) "nLocalAddress: " + nLocalAddress); // 5988
}

void OmxStorageMgr::saveToStorage(void)
{
	// Serial.println("Saving to Storage...");
	saveHeader();
	savePatterns();
}

bool OmxStorageMgr::loadFromStorage(void)
{
	// This load can happen soon after Serial.begin
	// - enable this 'wait for Serial' if you need to Serial.print during loading
	// while( !Serial );

	// Serial.println("Read the header");
	bool bContainedData = loadHeader();

	if (bContainedData)
	{
		// Serial.println("Loading patterns");
		loadPatterns();
		OmxModeMgr::changeOmxMode(sysSettings.omxMode);

		omxDisp.isDirty();
		omxLeds.isDirty();
		return true;
	}

	// Serial.println("-- Failed to load --");

	omxDisp.isDirty();
	omxLeds.isDirty();

	return false;
}
