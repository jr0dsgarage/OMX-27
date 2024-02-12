#pragma once
#include "midimacro_interface.h"
#include "../utils/PotPickupUtil.h"

namespace midimacro
{
	struct MidiParamBank
	{
		const char *bankName;
		uint8_t midiCCs[5] = {255, 255, 255, 255, 255};
		uint8_t midiValues[5] = {0, 0, 0, 0, 0};
		const char *paramNames[5];

		void SetCCs(
			const char *nameA = "", uint8_t a = 255,
			const char *nameB = "", uint8_t b = 255,
			const char *nameC = "", uint8_t c = 255,
			const char *nameD = "", uint8_t d = 255,
			const char *nameE = "VOL", uint8_t e = 7)
		{
			SetCC(0, nameA, a);
			SetCC(1, nameB, b);
			SetCC(2, nameC, c);
			SetCC(3, nameD, d);
			SetCC(4, nameE, e);
		}

		void SetCC(uint8_t index, const char *name, uint8_t cc)
		{
			if (index >= 5)
				return;
			midiCCs[index] = cc;
			paramNames[index] = name;
		}

		int8_t ContainsCC(uint8_t cc)
		{
			for(int8_t i = 0; i < 5; i++)
			{
				if(midiCCs[i] == cc)
				{
					return i;
				}
			}
			return -1;
		}

		int8_t UpdateCCValue(uint8_t cc, uint8_t value)
		{
			int8_t index = ContainsCC(cc);
			if(index < 0) return index;
			midiValues[index] = value;
			return index;
		}
	};

	class MidiMacroDeluge : public MidiMacroInterface
	{
	public:
		MidiMacroDeluge();
		~MidiMacroDeluge() {}

		bool consumesPots() override { return true; }
		bool consumesDisplay() override { return true; }

		String getName() override;

		void loopUpdate() override;

		void onDisplayUpdate() override;

		void onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta) override;
		void onKeyUpdate(OMXKeypadEvent e) override;
		void drawLEDs() override;

		void inMidiControlChange(byte channel, byte control, byte value) override;
	protected:
		void onEnabled() override;
		void onDisabled() override;

		void onEncoderChangedEditParam(Encoder::Update enc) override;

	private:
		static const uint8_t kNumBanks = 4;

		MidiParamBank* getActiveBank();
		MidiParamBank paramBanks[kNumBanks];
		// Maps each CC to a cached value
		uint8_t delVals[127];

		PotPickupUtil potPickups[5];

		// bool m8mutesolo_[16];

		// Control key mappings
		static const uint8_t keyFilt1_ = 3;
		static const uint8_t keyFilt2_ = 4;
		static const uint8_t keyEnv1_ = 1;
		static const uint8_t keyEnv2_ = 2;
	};
}
