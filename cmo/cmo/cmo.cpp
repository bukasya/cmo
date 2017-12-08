#include "stdafx.h"
#include "manager.h"
#include <vector>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <ctime>

using namespace std;

#define SOURCES_NUMBER 7
#define HANDLERS_NUMBER 5
#define BUFFER_SIZE 2
#define MAX_GENERATION_TIME 10000
#define MIN_GENERATION_TIME 3000

vector<Source*> sources;
vector<Handler*> handlers;
deque<Request> reqs;
mutex handlers_mutex;
mutex reqs_mutex;
//to sync handling operations
mutex handling_mutex[HANDLERS_NUMBER];
condition_variable handling_permission;
bool there_is_req_to_handle;
Request req_to_handle;
//to sync placing operations
mutex placing_mutex[SOURCES_NUMBER];
condition_variable placing_permission;
bool there_is_req_to_put;
Request req_to_put;
bool placing_manager_is_free;

vector<Event> events;
mutex events_mutex;

void source_func()
{
	Source *source = new Source(sources.size()); // create a source for every source_thread
	sources.push_back(source);

	unique_lock<mutex> placing_lock(placing_mutex[source->get_number()]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(MIN_GENERATION_TIME, MAX_GENERATION_TIME);

	for (;;)
	{
		// simulate generation
		int random = dis(gen);
		this_thread::sleep_for(std::chrono::milliseconds(random));
		Request r = source->generate_request();
		
		events_mutex.lock();
		events.push_back(Event("Source " + to_string(source->get_number()) + " generated request at " + to_string(r.get_generation_time()) + "\n"));
		events_mutex.unlock();
		placing_permission.wait(placing_lock, [] {return placing_manager_is_free; });

		req_to_put = r;
		there_is_req_to_put = true;
	}
}

void handler_func()
{
	Handler *handler = new Handler(handlers.size()); // create a handler for every handler_thread
	handlers.push_back(handler);

	unique_lock<mutex> handling_lock(handling_mutex[(handler->get_number())]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::exponential_distribution<> dis(0.001);

	for (;;)
	{
		handling_permission.wait(handling_lock, [] {return there_is_req_to_handle; });

		handler->set_is_busy(true);

		there_is_req_to_handle = false;

		// simulate handling
		double time = dis(gen) + MIN_GENERATION_TIME;
		events_mutex.lock();
		events.push_back(Event("Handler " + to_string(handler->get_number()) + " started to proceed\n")); // add time
		events_mutex.unlock();
		handler->handle(req_to_handle, (int)time);
		events_mutex.lock();
		events.push_back(Event("Handler " + to_string(handler->get_number()) + " finished proceeding on request " + to_string(req_to_handle.get_source_number()) + " " +to_string(req_to_handle.get_generation_time()) + "\n"));
		events_mutex.unlock();

		handlers_mutex.lock();
		handler->set_is_busy(false);
		handlers_mutex.unlock();
	}
}

void placing_manager_func()
{
	placing_manager_is_free = true;
	placing_permission.notify_one();

	//std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	for (;;)
	{
		if (there_is_req_to_put)
		{
			placing_manager_is_free = false;
			Request r = req_to_put;
			there_is_req_to_put = false;

			reqs_mutex.lock();

			if (reqs.size() < BUFFER_SIZE) // place request into buffer if it's not full
			{
				reqs.push_back(r);
				reqs_mutex.unlock();
				events_mutex.lock();
				events.push_back(Event("Put a request in buffer: source: " + to_string(r.get_source_number()) + ", time: " + to_string(r.get_generation_time()) + "\n"));
				events_mutex.unlock();
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
					events_mutex.lock();
					events.push_back(Event("Request is removed: " + to_string((*req_to_remove).get_source_number()) + ", time: " + to_string((*req_to_remove).get_generation_time()) + "\n"));
					events_mutex.unlock();
					reqs.erase(reqs.begin() + curr_req);
					reqs.push_back(r);

					events_mutex.lock();
					events.push_back(Event("Put a request in buffer: Source: " + to_string(r.get_source_number()) + ", time: " + to_string(r.get_generation_time()) + "\n"));
					events_mutex.unlock();
				}
				else { // else if initial request has to be removed, just ignore it
					events_mutex.lock();
					events.push_back(Event("Request is ignored: " + to_string(r.get_source_number()) + ", time: " + to_string(r.get_generation_time()) + "\n"));
					events_mutex.unlock();
				}
				reqs_mutex.unlock();
			}
			placing_manager_is_free = true;
			placing_permission.notify_one();
		}
	}
}

void handling_manager_func()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // sleep to increase chance that there will be requests to handle

	//handlers_mutex.lock();
	vector<Handler*>::iterator current_handler = handlers.begin(); // iterator to iterate handlers vector
	//handlers_mutex.unlock();

	for (;;) {
		int times_to_do_loop = HANDLERS_NUMBER; // make sure that none of existing handlers is free, then go on
		reqs_mutex.lock();

		if (reqs.size() > 0) // if there is a request to handle
		{
			handlers_mutex.lock();
			while ((*current_handler)->get_is_busy() && times_to_do_loop > 0) // find next free handler
			{
				times_to_do_loop--;
				if (current_handler + 1 == handlers.end()) // loop a vector
				{
					current_handler = handlers.begin();
				}
				else
				{
					current_handler++;
				}
			}
			if (!(*current_handler)->get_is_busy()) // if there if a free handler, give it a request to handle
			{
				req_to_handle = reqs.front();
				reqs.pop_front(); // remove handling request from buffer
				reqs_mutex.unlock();
				there_is_req_to_handle = true;

				handling_permission.notify_one(); // notify that free handler (but i'm not sure it works as intended??)
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
}


int main()
{
	vector<thread> source_threads;
	vector<thread> handler_threads;

	srand((int)&handlers);

	thread(handling_manager_func).detach();
	thread(placing_manager_func).detach();


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

	for (;;)
	{

	}

    return 0;
}