#include "stdafx.h"
#include "handler.h"

using namespace std;


Handler::Handler(int handler_number)
{
	number = handler_number;
	is_busy = false;
	uptime = 0;
}

Handler::~Handler()
{
	// TODO
}

int Handler::get_number()
{
	return number;
}

bool Handler::get_is_busy()
{
	return this->is_busy;
}

void Handler::set_is_busy(bool value)
{
	this->is_busy = value;
}

int Handler::get_uptime()
{
	return this->uptime;
}

void Handler::set_uptime(int upt)
{
	this->uptime += upt;
}

void Handler::handle(Request r, int time)
{
	this_thread::sleep_for(std::chrono::milliseconds(time));
}
