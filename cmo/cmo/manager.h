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
};