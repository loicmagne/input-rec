#pragma once
#include "plugin-support.h"
#include <obs-module.h>

class InputWriter; // necessary forward declaration
class InputDevice {
public:
	virtual ~InputDevice() = default;
	virtual void write_header(InputWriter &writer) = 0;
	virtual void write_state(InputWriter &writer) = 0;
};