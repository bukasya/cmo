#include <ctime>

#pragma once
class Request
{
public:
	Request();
	Request(int number); //creating request with source number
	~Request();
	std::chrono::system_clock::time_point get_generation_time(); //it looks like just int? deal with this
	int get_source_number();
	std::chrono::duration<double> get_buffer_spent_time();
	void set_buffer_spent_time(std::chrono::duration<double> bs_time);
	std::chrono::duration<double> get_handling_time();
	void set_handling_time(std::chrono::duration<double> h_time);
	Request copy_req();

private:
	std::chrono::system_clock::time_point generation_time;
	int source_number;
	std::chrono::duration<double> buffer_spent_time;
	std::chrono::duration<double> handling_time;
};

