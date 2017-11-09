#include "request.h"
#include "event.h"
#pragma once
class Handler
{
public:
	Handler();
	~Handler();
	bool get_isBusy();
	Event handle(Request);
private:
	bool isBusy;
};