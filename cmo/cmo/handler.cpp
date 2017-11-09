#include "stdafx.h"
#include "handler.h"
#include "request.h"


Handler::Handler()
{
	// TODO
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
	this->is_busy = value;
}