#include "request.h"
#include "event.h"
#pragma once
class Handler
{
public:
	Handler();
	~Handler();
	bool get_is_busy();
	void set_is_busy(bool value);
private:
	bool is_busy;
};