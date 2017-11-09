#include "stdafx.h"
#include "request.h"
#include <ctime>

Request::Request()
{
	//do I really need this?
}

Request::Request(int source_number)
{
	Request::generation_time = time(0);
	Request::source_number = source_number;
}


Request::~Request()
{
	// TODO
	//delete[] &generation_time;
	//delete[] &source_number;
}

time_t Request::get_generation_time()
{
	return generation_time;
}

int Request::get_source_number()
{
	return source_number;
}

