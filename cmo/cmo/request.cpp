#include "stdafx.h"
#include "request.h"
#include <ctime>

Request::Request()
{
	//do I really need this?
}

Request::Request(int source_number)
{
	Request::generation_time = std::chrono::system_clock::now();
	Request::source_number = source_number;
}

Request::~Request()
{
	// TODO
}

std::chrono::system_clock::time_point Request::get_generation_time()
{
	return generation_time;
}

int Request::get_source_number()
{
	return source_number;
}

std::chrono::duration<double> Request::get_buffer_spent_time()
{
	return this->buffer_spent_time;
}

void Request::set_buffer_spent_time(std::chrono::duration<double> bs_time)
{
	this->buffer_spent_time = bs_time;
}

std::chrono::duration<double> Request::get_handling_time()
{
	return this->handling_time;
}

void Request::set_handling_time(std::chrono::duration<double> h_time)
{
	this->handling_time = h_time;
}

Request Request::copy_req()
{
	Request new_req = Request();
	new_req.source_number = this->get_source_number();
	new_req.generation_time = this->get_generation_time();
	return new_req;
}