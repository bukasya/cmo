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
	time_t get_buffer_spent_time();
	void set_buffer_spent_time(time_t bs_time);
	time_t get_handling_time();
	void set_handling_time(time_t h_time);

private:
	time_t generation_time;
	int source_number;
	time_t buffer_spent_time;
	time_t handling_time;
};

