#include "request.h"
#include "event.h"
#pragma once
class Handler
{
public:
	Handler(int handler_number);
	~Handler();
	int get_number();
	bool get_is_busy();
	void set_is_busy(bool value);
private:
	int number;
	bool is_busy;
};