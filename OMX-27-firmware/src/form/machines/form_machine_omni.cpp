#include "form_machine_omni.h"

FormMachineOmni::FormMachineOmni()
{
}
FormMachineOmni::~FormMachineOmni()
{
}

FormMachineInterface * FormMachineOmni::getClone()
{
    auto clone = new FormMachineOmni();
    return clone;
}

void FormMachineOmni::onEnabled()
{
}
void FormMachineOmni::onDisabled()
{
}

void FormMachineOmni::onEncoderChangedSelectParam(Encoder::Update enc)
{
}
void FormMachineOmni::onEncoderChangedEditParam(Encoder::Update enc)
{
}

void FormMachineOmni::onPotChanged(int potIndex, int prevValue, int newValue, int analogDelta)
{
}
void FormMachineOmni::onClockTick()
{
}
void FormMachineOmni::loopUpdate()
{
}
bool FormMachineOmni::updateLEDs()
{
    return true;
}
void FormMachineOmni::onEncoderButtonDown()
{
}
bool FormMachineOmni::onKeyUpdate(OMXKeypadEvent e)
{
    return true;
}
bool FormMachineOmni::onKeyHeldUpdate(OMXKeypadEvent e)
{
    return true;
}
void FormMachineOmni::onDisplayUpdate()
{
}

// AUX + Top 1 = Play Stop
// For Omni:
// AUX + Top 2 = Reset
// AUX + Top 3 = Flip play direction if forward or reverse
// AUX + Top 4 = Increment play direction mode
void FormMachineOmni::onAUXFunc(uint8_t funcKey) {}