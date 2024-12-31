#pragma once

class InputWriter; // necessary forward declaration
class InputDevice {
public:
	virtual ~InputDevice() = default;
	virtual void write_header(InputWriter &writer) = 0;
	virtual void write_state(InputWriter &writer) = 0;
	virtual void loop(InputWriter &writer) = 0;
};