#include "stdafx.h"
#include "handler.h"

using namespace std;


Handler::Handler(int handler_number)
{
	number = handler_number;
	is_busy = false;
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

void Handler::handle(Request r, int time)
{
	cout << "Handler " << this->get_number() << " started to proceed.\n";
	this_thread::sleep_for(std::chrono::milliseconds(time));
	cout << "Handler" << this->get_number() << ": " << r.get_generation_time() << " " << r.get_source_number() << "\n";
}
