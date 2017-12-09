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
	int get_uptime();
	void set_uptime(int upt);
	void handle(Request r, int time);
private:
	int number;
	bool is_busy;
	int uptime;
};