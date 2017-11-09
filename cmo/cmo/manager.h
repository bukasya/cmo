#include "source.h"
#include "handler.h"
#include "request.h"
#include "event.h"

using namespace std;

#pragma once
class Manager
{
public:
	Manager();
	~Manager();
	void choose_free_handler();
	//Event place_request_to_handler(Request); //if there is a free handler
	//Event place_request_to_buffer(Request); //if there is no free handlers
};

