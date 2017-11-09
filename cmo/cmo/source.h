#include "request.h"

#pragma once
class Source
{
public:
	Source(int number);
	~Source();
	Request generate_request();
	int get_number();

private:
	int number; //sets on creation
};

