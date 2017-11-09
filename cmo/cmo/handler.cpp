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

bool Handler::get_isBusy()
{
	return this->isBusy;
}

Event Handler::handle(Request request)
{
	// TODO
	//request.~Request();
	return Event();
}
