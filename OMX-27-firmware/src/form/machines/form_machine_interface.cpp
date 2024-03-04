#include "form_machine_interface.h"
#include "../../hardware/omx_disp.h"

void FormMachineInterface::setEnabled(bool newEnabled)
{
	enabled_ = newEnabled;
	if (enabled_)
	{
		onEnabled();
	}
	else
	{
		onDisabled();
	}
}
bool FormMachineInterface::isEnabled()
{
	return enabled_;
}

bool FormMachineInterface::getEncoderSelect()
{
	return encoderSelect_;
}

void FormMachineInterface::onEncoderChanged(Encoder::Update enc)
{
	if (getEncoderSelect())
	{
		onEncoderChangedSelectParam(enc);
	}
	else
	{
		onEncoderChangedEditParam(enc);
	}
}

// Handles selecting params using encoder
void FormMachineInterface::onEncoderChangedSelectParam(Encoder::Update enc)
{
	// params_.changeParam(enc.dir());
	omxDisp.setDirty();
}

void FormMachineInterface::onEncoderButtonDown()
{
	encoderSelect_ = !encoderSelect_;
	omxDisp.setDirty();
}
