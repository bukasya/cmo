#include "source.h"
#include "handler.h"
#include "request.h"
#include "event.h"

using namespace std;

// perhaps i don't need this class?
#pragma once
class Manager
{
public:
	Manager();
	~Manager();
	void choose_free_handler();
};