#include <ctime>

#pragma once
class Request
{
public:
	Request();
	Request(int number); //creating request with source number
	~Request();
	time_t get_generation_time(); //it looks like just int? deal with this
	int get_source_number();

private:
	time_t generation_time;
	int source_number;
};

