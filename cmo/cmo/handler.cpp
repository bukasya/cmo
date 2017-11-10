#include "stdafx.h"
#include "handler.h"
#include "request.h"


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