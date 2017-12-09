#include "stdafx.h"
#include "manager.h"
#include <vector>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <ctime>
#include <atomic>

using namespace std;

#define SOURCES_NUMBER 10
#define HANDLERS_NUMBER 10
#define BUFFER_SIZE 2
#define MAX_GENERATION_TIME 10000
#define MIN_GENERATION_TIME 3000
#define REQS_TO_GEN 10

vector<Source*> sources;
vector<Handler*> handlers;
deque<Request*> reqs;
vector<Event> events;
mutex events_mutex;
mutex handlers_mutex;
mutex reqs_mutex;
//to sync handling operations
mutex handling_mutex[HANDLERS_NUMBER];
condition_variable handling_permission[HANDLERS_NUMBER];
bool there_is_req_to_handle;
Request *req_to_handle;
//to sync placing operations
mutex placing_mutex[SOURCES_NUMBER];
condition_variable placing_permission;
bool there_is_req_to_put;
Request *req_to_put;
mutex req_to_put_mutex;
bool placing_manager_is_free = true;

atomic<int> all_reqs = 0;
atomic<int> handled_reqs = 0;
atomic<int> existing_reqs = 0;

atomic<int> handlers_finished = 0;
atomic<int> sources_finished = 0;
bool pm_is_done = false;
bool hm_is_done = false;

void source_func()
{
	Source *source = new Source(sources.size()); // create a source for every source_thread
	sources.push_back(source);

	unique_lock<mutex> placing_lock(placing_mutex[source->get_number()]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(MIN_GENERATION_TIME, MAX_GENERATION_TIME);
	Request r;

	while (all_reqs < REQS_TO_GEN)
	{
		// simulate generation
		int random = dis(gen);
		this_thread::sleep_for(std::chrono::milliseconds(random));
		r = source->generate_request();
		
		events_mutex.lock();
		events.push_back(Event("Source " + to_string(source->get_number()) + " generated request at " + to_string(r.get_generation_time()) + "\n"));
		events_mutex.unlock();

		all_reqs++;
		placing_permission.wait(placing_lock, [] {return placing_manager_is_free; });

		req_to_put_mutex.lock();
		req_to_put = &r;
		req_to_put_mutex.unlock();
		there_is_req_to_put = true;
	}
	sources_finished++;
	while(!(hm_is_done && pm_is_done))
	{
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

	while (!hm_is_done)
	{
		if ((handling_permission[handler->get_number()].wait_for(handling_lock, chrono::milliseconds(1000))) != cv_status::timeout && there_is_req_to_handle)
		{
			Request r = *req_to_handle;
			handler->set_is_busy(true);
			there_is_req_to_handle = false;

			// simulate handling
			int handle_time = (int)dis(gen) + MIN_GENERATION_TIME;
			events_mutex.lock();
			events.push_back(Event("Handler " + to_string(handler->get_number()) + " started to proceed\n")); // add time
			events_mutex.unlock();
			handler->handle(r, handle_time);
			handler->set_uptime(handle_time/1000);
			events_mutex.lock();
			events.push_back(Event("Handler " + to_string(handler->get_number()) + " finished proceeding on request " + to_string(r.get_source_number()) + " " + to_string(r.get_generation_time()) + "\n"));
			events_mutex.unlock();

			handled_reqs++;

			handlers_mutex.lock();
			handler->set_is_busy(false);
			handlers_mutex.unlock();
		}
	}
	handlers_finished++;
}

void placing_manager_func()
{
	mutex pmp_mutex;
	unique_lock<mutex> pmp_lock(pmp_mutex);
	placing_manager_is_free = true;
	placing_permission.notify_one();

	while (sources_finished < sources.size())
	{
		if (there_is_req_to_put)
		{
			placing_manager_is_free = false;
			req_to_put_mutex.lock();
			Request *r = req_to_put;
			req_to_put_mutex.unlock();
			there_is_req_to_put = false;

			reqs_mutex.lock();

			if (reqs.size() < BUFFER_SIZE) // place request into buffer if it's not full
			{
				reqs.push_back(r);
				reqs_mutex.unlock();
				events_mutex.lock();
				events.push_back(Event("Put a request in buffer: source: " + to_string(r->get_source_number()) + ", time: " + to_string(r->get_generation_time()) + "\n"));
				events_mutex.unlock();

				existing_reqs++;
			}
			else // delete request from buffer or do not place new request in it if buffer is full
			{
				Request *req_to_remove = r;
				int i = 0;
				int curr_req = 0;
				for (Request *req : reqs) // find request to remove (with highest source number)
				{
					if (req->get_source_number() > (*req_to_remove).get_source_number()) // if current req's source is higher than req_to_remove's then it's the new req_to_remove
					{
						req_to_remove = req;
						curr_req = i;
					}
					else if (req->get_source_number() == (*req_to_remove).get_source_number()) // if current req's source is equal to req_to_remove's source than compare generation time
					{
						if (req->get_generation_time() > (*req_to_remove).get_generation_time()) // if current req's gen time is higher then req_to_remove's then it's the new req_to_remove
						{
							req_to_remove = req;
							curr_req = i;
						}
					}
					i++;
				}
				if (req_to_remove != r) // if req_to_remove is not an initial request then remove it from buffer and place here a new request instead
				{
					events_mutex.lock();
					events.push_back(Event("Request is removed: " + to_string((*req_to_remove).get_source_number()) + ", time: " + to_string((*req_to_remove).get_generation_time()) + "\n"));
					events_mutex.unlock();
					reqs.erase(reqs.begin() + curr_req);
					reqs.push_back(r);

					events_mutex.lock();
					events.push_back(Event("Put a request in buffer: Source: " + to_string(r->get_source_number()) + ", time: " + to_string(r->get_generation_time()) + "\n"));
					events_mutex.unlock();
				}
				else { // else if initial request has to be removed, just ignore it
					events_mutex.lock();
					events.push_back(Event("Request is ignored: " + to_string(r->get_source_number()) + ", time: " + to_string(r->get_generation_time()) + "\n"));
					events_mutex.unlock();
				}
				reqs_mutex.unlock();
			}
			placing_manager_is_free = true;
			placing_permission.notify_one();
		}
	}
	pm_is_done = true;
}

void handling_manager_func()
{
	vector<Handler*>::iterator current_handler = handlers.begin(); // iterator to iterate handlers vector

	while (!(reqs.empty() && all_reqs >= REQS_TO_GEN))
	{
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

				handling_permission[(*current_handler)->get_number()].notify_one(); // notify that free handler (but i'm not sure it works as intended??)
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
	hm_is_done = true;
}


int main()
{
	vector<thread*> source_threads;
	vector<thread*> handler_threads;

	srand((int)&handlers);

	for (int srcs = 2; srcs <= 5; srcs++)
	{
		for (int hnds = 5; hnds <= 5; hnds++)
		{
			for (int i = 0; i < srcs; i++) // create source threads
			{
				thread *t = new thread(source_func);
				source_threads.push_back(t);
				t->detach();
			}
			thread(placing_manager_func).detach();
			this_thread::sleep_for(chrono::milliseconds(5000));
			for (int i = 0; i < hnds; i++) // create source threads
			{
				thread *t = new thread(handler_func);
				handler_threads.push_back(t);
				t->detach();
			}
			thread(handling_manager_func).detach();

			cout << "Sources: " << srcs << ", " << "handlers: " << hnds << "\n";
			time_t start = time(0);

			while (handlers_finished < handlers.size())
			{
			}

			time_t finish = time(0);
			time_t est_time = finish - start;
			cout << "Estimated time: " << est_time << "\n";
			cout << "AR = " << all_reqs << "; HR = " << handled_reqs << "; ER = " << existing_reqs << "\n";

			for each (Handler *h in handlers)
			{
				int h_upt = h->get_uptime();
				cout << "Handler's " << h->get_number() << " uptime = " << h_upt << ". Efficiency = " << (double)h_upt/est_time << "\n";
			}
			
			handlers_finished = 0;
			sources_finished = 0;
			all_reqs = 0;
			handled_reqs = 0;
			existing_reqs = 0;
			sources.clear();
			handlers.clear();
			reqs.clear();
			hm_is_done = false;
			pm_is_done = false;
		}
	}
    return 0;
}