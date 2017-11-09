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
//for debugging
#include <iostream> 
#include <cstdlib>
#include <stdlib.h>

using namespace std;

#define SOURCES_NUMBER 4
#define HANDLERS_NUMBER 1
#define BUFFER_SIZE 2
#define MAX_GENERATION_TIME 10000
#define MIN_GENERATION_TIME 3000

int source_num = 1;
queue<Source> srcs;
queue<Source> sources;
queue<Handler> handlers;
deque<Request> reqs;
mutex sources_mutex;
mutex handlers_mutex;
mutex reqs_mutex;
mutex source_num_mutex;

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
		Request r = source.generate_request();
		reqs_mutex.lock();

		if (reqs.size() < BUFFER_SIZE) // place request into buffer if it's not full
		{
			reqs.push_back(r);
			reqs_mutex.unlock();
			cout << "Source" << r.get_source_number() << " put a request in buffer: " << r.get_generation_time() << "\n";
		}
		else // delete request from buffer or do not place new request in it if buffer is full
		{
			Request *req_to_remove = &r;
			int i = 0;
			int curr_req = 0;
			for (Request &req : reqs) // find request to remove (with highest source number)
			{
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
				cout << "Request is removed: " << (*req_to_remove).get_source_number() << " " << (*req_to_remove).get_generation_time() << "\n";
				reqs.erase(reqs.begin() + curr_req);
				reqs.push_back(r);
				cout << "Source" << r.get_source_number() << " put a request in buffer: " << r.get_generation_time() << "\n";
			}
			else { // else if initial request has to be removed, just ignore it
				cout << "Request is ignored: " << (*req_to_remove).get_source_number() << " " << (*req_to_remove).get_generation_time() << "\n";
			}
			reqs_mutex.unlock();
		}
	}
}

void handler_func()
{
	Handler handler = Handler();
	handlers_mutex.lock();
	handlers.push(handler);
	handlers_mutex.unlock();
	Request r = NULL;

	std::random_device rd;
	std::mt19937 gen(rd());
	std::exponential_distribution<> dis(0.001);

	for (;;)
	{
		reqs_mutex.lock();

		if (reqs.size() > 0)
		{
			r = reqs.front();
			reqs.pop_front();
			reqs_mutex.unlock();
			double random = dis(gen) + MIN_GENERATION_TIME;
			this_thread::sleep_for(std::chrono::milliseconds((int)random));

			cout << "Handler: " << r.get_generation_time() << " " << r.get_source_number() << "\n";
		}
		else
		{
			reqs_mutex.unlock();
		}
	}
}


int main()
{
	vector<thread> source_threads;
	vector<thread> handler_threads;
	//vector<Source> sources;
	vector<Handler> handlers;
	vector<Request> buffer;
	Manager manager = Manager();

	srand((int)&handlers);

	for (int i = 0; i < SOURCES_NUMBER; i++)
	{
		//thread t = thread(source_func);
		//t.detach();
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

	std::this_thread::sleep_for(std::chrono::milliseconds(60000)); //debug (watching requests in vector)

    return 0;
}

