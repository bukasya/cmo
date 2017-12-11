#include "request.h"

#pragma once
class Source
{
public:
	Source(int number);
	~Source();
	Request generate_request();
	int get_number();
	int get_generated_reqs();
	void increment_generated_reqs();
	int get_declined_reqs();
	void increment_declined_reqs();
private:
	int number; //sets on creation
	int generated_reqs;
	int declined_reqs;
};

