#include "omni_note_editor.h"
#include "../../config.h"
#include "../../consts/colors.h"
#include "../../utils/omx_util.h"
#include "../../hardware/omx_disp.h"
#include "../../hardware/omx_leds.h"
#include <algorithm>

namespace FormOmni
{
    enum NoteEditorPage
    {
        NEDITPAGE_1, // BPM
        OMNIPAGE_COUNT
    };

    OmniNoteEditor::OmniNoteEditor()
    {
    }

    OmniNoteEditor::~OmniNoteEditor()
    {
    }

    uint8_t OmniNoteEditor::getSelStep()
    {
        return selStep_;
    }

    void OmniNoteEditor::setSelStep(uint8_t stepIndex)
    {
        selStep_ = stepIndex;
    }

    void OmniNoteEditor::updateLEDs(Track *track)
    {
        omxLeds.drawMidiLeds(omxFormGlobal.musicScale);
    }
    void OmniNoteEditor::onKeyUpdate(OMXKeypadEvent e, Track *track)
    {
        if(e.held()) return;

        uint8_t thisKey = e.key();

        if(thisKey == 0) return;

        if(e.down())
        {
            std::vector<uint8_t> heldKeys;

            for(uint8_t i = 1; i < 27; i++)
            {
                if(midiSettings.keyState[i])
                {
                    heldKeys.push_back(i);
                }
            }

            std::sort(heldKeys.begin(), heldKeys.end());

            uint8_t keyIndex = 0;

            std::vector<uint8_t> noteNumbers;

            for (uint8_t i = 0; i < heldKeys.size(); i++)
            {
                int8_t noteNumber = omxUtil.getNoteNumber(heldKeys[keyIndex], omxFormGlobal.musicScale);

                if (noteNumber >= 0 && noteNumber <= 127)
                {
                    noteNumbers.push_back(noteNumber);
                }
            }

            for(uint8_t i = 0; i < 6; i++)
            {
                if(i < noteNumbers.size())
                {
                    track->steps[selStep_].notes[i] = noteNumbers[i];
                }
                else
                {
                    track->steps[selStep_].notes[i] = -1;
                }
            }
        }
    }

    void OmniNoteEditor::onKeyHeldUpdate(OMXKeypadEvent e, Track *track)
    {
    }
    void OmniNoteEditor::onDisplayUpdate(Track *track)
    {
        auto notes = track->steps[selStep_].notes;

        // omxDisp.setLegend(0, "NT 1", notes[0]);
        // omxDisp.setLegend(0, "NT 2", notes[0]);
        // omxDisp.setLegend(0, "NT 3", notes[0]);
        // omxDisp.setLegend(0, "NT 4", notes[0]);

        tempString = "";
        // notesString2 = "";
        int dispNotes[6];
        int8_t rootNote = -1;

        for (uint8_t i = 0; i < 6; i++)
        {
            int8_t note = notes[i];
            dispNotes[i] = note;

            if (note >= 0 && note <= 127)
            {
                if (rootNote < 0 || note < rootNote)
                {
                    rootNote = note;
                }

                if (i > 0)
                {
                    tempString.append(" ");
                }
                tempString.append(omxFormGlobal.musicScale->getFullNoteName(note));
            }
        }

        tempStrings[0] = String(selStep_ + 1);

        const char *labels[2];
        labels[0] = tempStrings[0].c_str();
        labels[1] = tempString.c_str();

        omxDisp.dispKeyboard(rootNote, dispNotes, true, labels, 1);

        // omxDisp.dispGenericModeLabel("Note Editor");
    }
    void OmniNoteEditor::onEncoderChangedSelectParam(Encoder::Update enc, Track *track)
    {
    }
    void OmniNoteEditor::onEncoderChangedEditParam(Encoder::Update enc, Track *track)
    {
    }

    OmniNoteEditor omniNoteEditor;
}