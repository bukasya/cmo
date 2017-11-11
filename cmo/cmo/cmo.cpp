#include "stdafx.h"
#include "manager.h"
#include <vector>
#include <mutex>
#include <queue>
#include <random>

using namespace std;

#define SOURCES_NUMBER 7
#define HANDLERS_NUMBER 5
#define BUFFER_SIZE 2
#define MAX_GENERATION_TIME 10000
#define MIN_GENERATION_TIME 3000

int source_num = 1;
int handler_num = 0;
vector<Source> sources;
vector<Handler> handlers;
deque<Request> reqs;
mutex sources_mutex;
mutex handlers_mutex;
mutex reqs_mutex;
mutex source_num_mutex;
mutex handler_num_mutex;
mutex placing_mutex[HANDLERS_NUMBER];
condition_variable handling_permission[HANDLERS_NUMBER];
bool there_is_req_to_handle;
Request req_to_handle;

void source_func()
{
	source_num_mutex.lock();
	Source source = Source(source_num); // create a source for every source_thread
	source_num++;
	source_num_mutex.unlock();
	sources_mutex.lock();
	sources.push_back(source);
	sources_mutex.unlock();

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(MIN_GENERATION_TIME, MAX_GENERATION_TIME);

	for (;;)
	{
		// simulate generation
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
			// make a func to decide which request should be removed
			// and this func should be in manager perhaps?
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
	handler_num_mutex.lock();
	Handler handler = Handler(handler_num); // create a handler for every handler_thread
	handler_num++;
	handler_num_mutex.unlock();
	handlers_mutex.lock();
	handlers.push_back(handler);
	handlers_mutex.unlock();

	unique_lock<mutex> handling_lock(placing_mutex[handler.get_number()]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::exponential_distribution<> dis(0.001);

	for (;;)
	{
		handling_permission[handler.get_number()].wait(handling_lock, [] {return there_is_req_to_handle; });

		handlers[handler.get_number()].set_is_busy(true);

		there_is_req_to_handle = false;

		// simulate handling
		double time = dis(gen) + MIN_GENERATION_TIME;
		handler.handle(req_to_handle, (int)time);

		handlers_mutex.lock();
		handlers[handler.get_number()].set_is_busy(false); // free the handler
		handlers_mutex.unlock();
	}
}


int main()
{
	vector<thread> source_threads;
	vector<thread> handler_threads;

	srand((int)&handlers);

	for (int i = 0; i < SOURCES_NUMBER; i++) // create source threads
	{
		source_threads.push_back(thread(source_func));
	}

	for (int i = 0; i < source_threads.size(); i++)
	{
		source_threads[i].detach();
	}

	for (int i = 0; i < HANDLERS_NUMBER; i++) // create handler threads
	{
		handler_threads.push_back(thread(handler_func));
	}

	for (int i = 0; i < handler_threads.size(); i++)
	{
		handler_threads[i].detach();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // sleep to increase chance that there will be requests to handle

	handlers_mutex.lock();
	vector<Handler>::iterator current_handler = handlers.begin(); // iterator to iterate handlers vector
	handlers_mutex.unlock();

	for (;;) {
		int times_to_do_loop = HANDLERS_NUMBER; // make sure that none of existing handlers is free, then go on
		reqs_mutex.lock();

		if (reqs.size() > 0) // if there is a request to handle
		{
			handlers_mutex.lock();
			while ((*current_handler).get_is_busy() && times_to_do_loop > 0) // find next free handler
			{
				times_to_do_loop--;
				if (current_handler+1 == handlers.end()) // loop a vector
				{
					current_handler = handlers.begin();
				}
				else
				{
					current_handler++;
				}
			}
			if (!(*current_handler).get_is_busy()) // if there if a free handler, give it a request to handle
			{
				//(*current_handler).set_is_busy(true);
				req_to_handle = reqs.front();
				reqs.pop_front(); // remove handling request from buffer
				reqs_mutex.unlock();
				there_is_req_to_handle = true;

				handling_permission[(*current_handler).get_number()].notify_one(); // notify that free handler (but i'm not sure it works as intended??)
			}
			else
			{
				reqs_mutex.unlock();
			}
			handlers_mutex.unlock();
		}
		else
		{

			reqs_mutex.unlock();
		}
		
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

    return 0;
}