// hate_this_cmo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "manager.h"
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <time.h>
#include <queue>
#include <random>
#include <iterator>
//for debugging
#include <iostream> 
#include <cstdlib>
#include <stdlib.h>

using namespace std;

#define SOURCES_NUMBER 2
#define HANDLERS_NUMBER 1
#define BUFFER_SIZE 2
#define MAX_GENERATION_TIME 10000
#define MIN_GENERATION_TIME 3000

int source_num = 1;
queue<Source> srcs;
queue<Source> sources;
vector<Handler> handlers;
deque<Request> reqs;
mutex sources_mutex;
mutex handlers_mutex;
mutex reqs_mutex;
mutex source_num_mutex;
mutex req_m; //mutex for getting request from main
condition_variable cv;
Request request_to_handle;
bool request_is_ready;
vector<Event> events;
mutex events_mutex;
string event_message;
Request r;
bool new_request_is_generated;
condition_variable new_req_cv;
mutex m; //???

void source_func()
{
	source_num_mutex.lock();
	Source source = Source(source_num);
	source_num++;
	source_num_mutex.unlock();
	sources_mutex.lock();
	sources.push(source);
	sources_mutex.unlock();

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(MIN_GENERATION_TIME, MAX_GENERATION_TIME);

	for (;;)
	{
		int random = dis(gen);
		this_thread::sleep_for(std::chrono::milliseconds(random));
		//Request r = source.generate_request();
		r = source.generate_request();
		new_request_is_generated = true;
		new_req_cv.notify_one();

		//reqs_mutex.lock();

		/*if (reqs.size() < BUFFER_SIZE) // place request into buffer if it's not full
		{
			reqs.push_back(r);
			reqs_mutex.unlock();
			events_mutex.lock();
			event_message = "Source" + to_string(r.get_source_number()) + " put a request in buffer: " + to_string(r.get_generation_time()) + "\n";
			events.push_back(Event(event_message));
			events_mutex.unlock();
			//cout << "Source" << r.get_source_number() << " put a request in buffer: " << r.get_generation_time() << "\n";
		}
		else // delete request from buffer or do not place new request in it if buffer is full
		{
			Request *req_to_remove = &r;
			int i = 0;
			int curr_req = 0;
			for (Request &req : reqs) // find request to remove (with highest source number)
			{
				//make a func
				if (req.get_source_number() > (*req_to_remove).get_source_number()) // if current req's source is higher than req_to_remove's then it's the new req_to_remove
				{
					req_to_remove = &req;
					curr_req = i;
				}
				else if (req.get_source_number() == (*req_to_remove).get_source_number()) // if current req's source is equal to req_to_remove's source than compare generation time
				{
					if (req.get_generation_time() > (*req_to_remove).get_generation_time()) // if current req's gen time is higher then req_to_remove's then it's the new req_to_remove
					{
						req_to_remove = &req;
						curr_req = i;
					}
				}
				i++;
			}
			if (req_to_remove != &r) // if req_to_remove is not an initial request then remove it from buffer and place here a new request instead
			{
				events_mutex.lock();
				event_message = "Request is removed: " + to_string((*req_to_remove).get_source_number()) + " " + to_string((*req_to_remove).get_generation_time()) + "\n";
				events.push_back(Event(event_message));
				//cout << "Request is removed: " << (*req_to_remove).get_source_number() << " " << (*req_to_remove).get_generation_time() << "\n";
				reqs.erase(reqs.begin() + curr_req);
				reqs.push_back(r);
				event_message = "Source" + to_string(r.get_source_number()) + " put a request in buffer: " + to_string(r.get_generation_time()) +"\n";
				events.push_back(Event(event_message));
				//cout << "Source" << r.get_source_number() << " put a request in buffer: " << r.get_generation_time() << "\n";
				events_mutex.unlock();
			}
			else { // else if initial request has to be removed, just ignore it
				events_mutex.lock();
				event_message = "Request is ignored: " + to_string((*req_to_remove).get_source_number()) + " " + to_string((*req_to_remove).get_generation_time()) + "\n";
				events.push_back(Event(event_message));
				//cout << "Request is ignored: " << (*req_to_remove).get_source_number() << " " << (*req_to_remove).get_generation_time() << "\n";
				events_mutex.unlock();
			}
			reqs_mutex.unlock();
		}*/

		//this_thread::sleep_for(std::chrono::milliseconds(5000));
	}
}

void handler_func()
{
	Handler handler = Handler();
	handlers_mutex.lock();
	handlers.push_back(handler);
	handlers_mutex.unlock();
	Request r = NULL;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::exponential_distribution<> dis(0.001);

	for (;;)
	{
		unique_lock<mutex> lk(req_m);
		cv.wait(lk, [] {return request_is_ready; });
		//reqs_mutex.lock();

		/*if (reqs.size() > 0)
		{
			r = reqs.front();
			reqs.pop_front();
			reqs_mutex.unlock();*/

		handlers_mutex.lock();
		handler.set_is_busy(true);
		handlers_mutex.unlock();

		r = request_to_handle;

		lk.unlock();

		//cout << "Handling... \n";

		double random = dis(gen) + MIN_GENERATION_TIME;
		this_thread::sleep_for(std::chrono::milliseconds((int)random));

		events_mutex.lock();
		event_message = "Handler: " + to_string(r.get_generation_time()) + " " + to_string(r.get_source_number()) + "\n";
		//cout << "Handler: " << r.get_generation_time() << " " << r.get_source_number() << "\n";
		events_mutex.unlock();

		handlers_mutex.lock();
		handler.set_is_busy(false);
		handlers_mutex.unlock();

		this_thread::sleep_for(std::chrono::milliseconds(200));
		/*}
		else
		{
			reqs_mutex.unlock();
		}*/
	}
}


int main()
{
	vector<thread> source_threads;
	vector<thread> handler_threads;

	srand((int)&handlers);

	for (int i = 0; i < SOURCES_NUMBER; i++)
	{
		source_threads.push_back(thread(source_func));
	}

	for (int i = 0; i < source_threads.size(); i++)
	{
		source_threads[i].detach();
	}

	for (int i = 0; i < HANDLERS_NUMBER; i++)
	{
		handler_threads.push_back(thread(handler_func));
	}

	for (int i = 0; i < handler_threads.size(); i++)
	{
		handler_threads[i].detach();
	}

	for (;;)
	{
		unique_lock<mutex> source_lock(m);
		new_req_cv.wait(source_lock, [] {return new_request_is_generated; });

		if (reqs.size() < BUFFER_SIZE) // place request into buffer if it's not full
		{
			reqs.push_back(r);
			reqs_mutex.unlock();
			events_mutex.lock();
			event_message = "Source" + to_string(r.get_source_number()) + " put a request in buffer: " + to_string(r.get_generation_time()) + "\n";
			events.push_back(Event(event_message));
			events_mutex.unlock();
			//cout << "Source" << r.get_source_number() << " put a request in buffer: " << r.get_generation_time() << "\n";
		}
		else // delete request from buffer or do not place new request in it if buffer is full
		{
			Request *req_to_remove = &r;
			int i = 0;
			int curr_req = 0;
			for (Request &req : reqs) // find request to remove (with highest source number)
			{
				//make a func
				if (req.get_source_number() > (*req_to_remove).get_source_number()) // if current req's source is higher than req_to_remove's then it's the new req_to_remove
				{
					req_to_remove = &req;
					curr_req = i;
				}
				else if (req.get_source_number() == (*req_to_remove).get_source_number()) // if current req's source is equal to req_to_remove's source than compare generation time
				{
					if (req.get_generation_time() > (*req_to_remove).get_generation_time()) // if current req's gen time is higher then req_to_remove's then it's the new req_to_remove
					{
						req_to_remove = &req;
						curr_req = i;
					}
				}
				i++;
			}
			if (req_to_remove != &r) // if req_to_remove is not an initial request then remove it from buffer and place here a new request instead
			{
				events_mutex.lock();
				event_message = "Request is removed: " + to_string((*req_to_remove).get_source_number()) + " " + to_string((*req_to_remove).get_generation_time()) + "\n";
				events.push_back(Event(event_message));
				//cout << "Request is removed: " << (*req_to_remove).get_source_number() << " " << (*req_to_remove).get_generation_time() << "\n";
				reqs.erase(reqs.begin() + curr_req);
				reqs.push_back(r);
				event_message = "Source" + to_string(r.get_source_number()) + " put a request in buffer: " + to_string(r.get_generation_time()) + "\n";
				events.push_back(Event(event_message));
				//cout << "Source" << r.get_source_number() << " put a request in buffer: " << r.get_generation_time() << "\n";
				events_mutex.unlock();
			}
			else { // else if initial request has to be removed, just ignore it
				events_mutex.lock();
				event_message = "Request is ignored: " + to_string((*req_to_remove).get_source_number()) + " " + to_string((*req_to_remove).get_generation_time()) + "\n";
				events.push_back(Event(event_message));
				//cout << "Request is ignored: " << (*req_to_remove).get_source_number() << " " << (*req_to_remove).get_generation_time() << "\n";
				events_mutex.unlock();
			}
			reqs_mutex.unlock();
		}
		//reqs_mutex.lock();
		if (reqs.size() > 0) {
			//create lock here and in handler_func to sync processes
			handlers_mutex.lock();
			for each (Handler h in handlers)
			{
				if (!h.get_is_busy())
				{
					request_to_handle = reqs.front();
					reqs.pop_front();
					reqs_mutex.unlock();
					request_is_ready = true;
					cv.notify_one();
					break;
				}

			}
			handlers_mutex.unlock();
		}
		else 
		{
			reqs_mutex.unlock();
		}

		this_thread::sleep_for(std::chrono::milliseconds(300));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(60000)); //debug (watching requests in vector)

	return 0;
}

