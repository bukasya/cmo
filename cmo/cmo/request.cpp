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
}

time_t Request::get_generation_time()
{
	return generation_time;
}

int Request::get_source_number()
{
	return source_number;
}

time_t Request::get_buffer_spent_time()
{
	return this->buffer_spent_time;
}

void Request::set_buffer_spent_time(time_t bs_time)
{
	this->buffer_spent_time = bs_time;
}

time_t Request::get_handling_time()
{
	return this->handling_time;
}

void Request::set_handling_time(time_t h_time)
{
	this->handling_time = h_time;
}