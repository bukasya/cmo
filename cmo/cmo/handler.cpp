#include "stdafx.h"
#include "handler.h"
#include "request.h"


Handler::Handler()
{
	is_busy = false;
}

Handler::~Handler()
{
	// TODO
}

bool Handler::get_is_busy()
{
	return this->is_busy;
}

void Handler::set_is_busy(bool value)
{
	is_busy = value;
}

/*Event Handler::handle(Request request)
{
	// TODO
	//request.~Request();
	//return Event();
}*/
