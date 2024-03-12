#pragma once

extern const int ROOTNOTECOLOR;
extern const int INSCALECOLOR;

class MusicScales
{
public:
	// int scaleDegrees[12];
	// int scaleOffsets[12];
	// int scaleColors[12];
	// const char* scaleNames[];
	// const char* noteNames[];
	// const int scalePatterns[][7];

	void calculateScaleIfModified(uint8_t scaleRoot, uint8_t scalePattern);
	void calculateScale(uint8_t scaleRoot, uint8_t scalePattern);
	static uint8_t getNumScales();
	// int scaleLength;

	// returns true if note 0-11 is in the currently calculated scale. NoteNum should be a midi note
	bool isNoteInScale(int8_t noteNum);

	// This takes a incoming note and forces it into the current scale
	int8_t remapNoteToScale(uint8_t noteNumber);

	// returns a note in the scale if key is one of the lower 16. Returns -1 otherwise.
	// TODO This won't work unless returns int, won't work with int8_t not sure why
	int getGroup16Note(uint8_t keyNum, int8_t octave);

	int8_t getNoteByDegree(uint8_t degree, int8_t octave);
	static uint8_t getDegreeFromNote(uint8_t noteNumber, int8_t rootNote, int8_t scalePatIndex);

	// Returns a color for the note
	int getScaleColor(uint8_t noteIndex);

	int getGroup16Color(uint8_t keyNum);

	static const char *getNoteName(uint8_t noteIndex, bool removeSpaces = false);
	static const char *getFullNoteName(uint8_t noteNumber);
	static const char *getScaleName(uint8_t scaleIndex);
	static const int8_t *getScalePattern(uint8_t patIndex);
	int getScaleLength();

private:
	bool scaleCalculated = false;
	bool scaleRemapCalculated_ = false;

	int8_t scaleOffsets[12];
	int8_t scaleDegrees[12];
	int scaleColors[12];
	uint8_t scaleLength = 0;

	int8_t scaleRemapper[12];

	int8_t rootNote;
	int8_t scaleIndex;

	void calculateRemap();

	int group16Offsets[16]; // 16 offsets for group16 mode
};
