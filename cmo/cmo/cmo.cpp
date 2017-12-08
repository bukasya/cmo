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

int source_num = 0;
int handler_num = 0;
vector<Source> sources;
vector<Handler> handlers;
deque<Request> reqs;
mutex sources_mutex;
mutex handlers_mutex;
mutex reqs_mutex;
mutex source_num_mutex;
mutex handler_num_mutex;
//to sync handling operations
mutex handling_mutex[10]; //HANDLERS_NUMBER
condition_variable handling_permission;
bool there_is_req_to_handle;
Request req_to_handle;
//to sync placing operations
mutex placing_mutex[10]; //SOURCES_NUMBER
condition_variable placing_permission;
bool there_is_req_to_put;
Request req_to_put;
bool placing_manager_is_free;

int reqs_number = 0;
mutex reqs_number_mutex;
int handled_reqs_number = 0;
mutex handled_reqs_number_mutex;
int existing_reqs_number = 0;
mutex existing_reqs_mutex;

vector<Event> events;
mutex events_mutex;

bool handle = true; //bool var for handlers proceeding loop
mutex handle_mutex;

//while these vars == true - managers are proceeding
bool handling_manager_proceed = true;
mutex handling_manager_proceed_mutex;
bool placing_manager_proceed = true;
mutex placing_manager_proceed_mutex;

condition_variable hm_proceeding_over;
condition_variable pm_proceeding_over;

int handlers_to_delete = 0;
mutex handlers_to_delete_mutex;

void source_func()
{
	source_num_mutex.lock();
	Source source = Source(source_num); // create a source for every source_thread
	source_num++;
	source_num_mutex.unlock();
	sources_mutex.lock();
	sources.push_back(source);
	sources_mutex.unlock();

	unique_lock<mutex> placing_lock(placing_mutex[source.get_number()]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(MIN_GENERATION_TIME, MAX_GENERATION_TIME);

	while (reqs_number < 10)
	{
		// simulate generation
		int random = dis(gen);
		this_thread::sleep_for(std::chrono::milliseconds(random));
		Request r = source.generate_request();

		events_mutex.lock();
		events.push_back(Event("Source " + to_string(source.get_number()) + " generated request at " + to_string(r.get_generation_time()) + "\n"));
		events_mutex.unlock();
		placing_permission.wait(placing_lock, [] {return placing_manager_is_free; });

		req_to_put = r;
		there_is_req_to_put = true;

		reqs_number_mutex.lock();
		reqs_number++;
		reqs_number_mutex.unlock();
	}

	handle_mutex.lock();
	handle = false;
	handle_mutex.unlock();
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

	unique_lock<mutex> handling_lock(handling_mutex[handler.get_number()]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::exponential_distribution<> dis(0.001);
	std::uniform_int_distribution<> sleep_dis(500, 1500);

	bool success = false;

	while (handle || handled_reqs_number < existing_reqs_number)
	{
		int sleep_time = sleep_dis(gen);
		//handling_permission.wait(handling_lock, [] {return there_is_req_to_handle; });
		if (handling_permission.wait_for(handling_lock, std::chrono::milliseconds(sleep_time)) != std::cv_status::timeout) //handling twice on some reqs
		{
			Request current_req_to_handle = req_to_handle;
			//handler.set_is_busy(true);
			handlers[handler.get_number()].set_is_busy(true);

			there_is_req_to_handle = false;
			time_t start_handling_time = time(0);

			// simulate handling
			double handling_time = dis(gen) + MIN_GENERATION_TIME;
			events_mutex.lock();
			events.push_back(Event("Handler " + to_string(handler.get_number()) + " started to proceed\n" + to_string(start_handling_time) + "\n"));
			events_mutex.unlock();
			handler.handle(current_req_to_handle, (int)handling_time);

			time_t end_handling_time = time(0);
			events_mutex.lock();
			events.push_back(Event("Handler " + to_string(handler.get_number()) + "finished proceeding on request " + to_string(current_req_to_handle.get_source_number())
				+ " " + to_string(current_req_to_handle.get_generation_time()) + " " + to_string(end_handling_time) + "\n"));
			events_mutex.unlock();

			handlers_mutex.lock();
			handlers[handler.get_number()].set_is_busy(false); // free the handler
			handlers_mutex.unlock();

			handled_reqs_number_mutex.lock();
			handled_reqs_number++;
			handled_reqs_number_mutex.unlock();
		}
	}
	handlers_to_delete_mutex.lock();
	handlers_to_delete++;
	handlers_to_delete_mutex.unlock();
}

void placing_manager_func()
{
	placing_manager_is_free = true;
	placing_permission.notify_one();

	//std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	while (sources.size() > 0)
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
				existing_reqs_mutex.lock();
				existing_reqs_number++;
				existing_reqs_mutex.unlock();
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
					else if (req.get_source_number() == (*req_to_remove).get_source_number()) // else if current req's source is equal to req_to_remove's source than compare generation time
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
	pm_proceeding_over.notify_one();
}

void handling_manager_func()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // sleep to increase chance that there will be requests to handle

	handlers_mutex.lock();
	vector<Handler>::iterator current_handler = handlers.begin(); // iterator to iterate handlers vector
	handlers_mutex.unlock();

	while (handlers.size() > 0) {
		int times_to_do_loop = HANDLERS_NUMBER; // make sure that none of existing handlers is free, then go on
		reqs_mutex.lock();

		if (reqs.size() > 0) // if there is a request to handle
		{
			handlers_mutex.lock();
			while ((*current_handler).get_is_busy() && times_to_do_loop > 0) // find next free handler
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
			if (!(*current_handler).get_is_busy()) // if there if a free handler, give it a request to handle
			{
				req_to_handle = reqs.front();
				reqs.pop_front(); // remove handling request from buffer
				reqs_mutex.unlock();
				there_is_req_to_handle = true;

				events_mutex.lock();
				events.push_back(Event("Manager gives: " + to_string(req_to_handle.get_source_number()) + " " + to_string(req_to_handle.get_generation_time()) + " to "
					+ to_string((*current_handler).get_number()) + "\n"));
				events_mutex.unlock();

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
	hm_proceeding_over.notify_one();
}


int main()
{
	srand((int)&handlers); //to make random numbers different each time the app starts
	mutex hm_mutex;
	unique_lock<mutex> hm_lock(hm_mutex);
	mutex pm_mutex;
	unique_lock<mutex> pm_lock(pm_mutex);

	for (int srcs_number = 3; srcs_number < 7; srcs_number++)
	{
		for (int hndlrs_number = 2; hndlrs_number < 7; hndlrs_number++)
		{
			vector<thread> source_threads;
			vector<thread> handler_threads;

			for (int i = 0; i < srcs_number; i++) // create source threads
			{
				source_threads.push_back(thread(source_func));
			}

			for (int i = 0; i < source_threads.size(); i++)
			{
				source_threads[i].detach();
			}

			for (int i = 0; i < hndlrs_number; i++) // create handler threads
			{
				handler_threads.push_back(thread(handler_func));
			}

			for (int i = 0; i < handler_threads.size(); i++)
			{
				handler_threads[i].detach();
			}

			this_thread::sleep_for(std::chrono::milliseconds(3000));

			thread placing_manager = thread(placing_manager_func);
			placing_manager.detach();
			thread handling_manager = thread(handling_manager_func);
			handling_manager.detach();

			cout << "Sources: " << srcs_number << ", " << "handlers: " << hndlrs_number << "\n";
			time_t start = time(0);

			while (handlers_to_delete < handlers.size())
			{
			}

			time_t finish = time(0);
			cout << "Estimated time: " << (finish - start) << "\n";
			cout << "AR = " << reqs_number << "; HR = " << handled_reqs_number << "; ER = " << existing_reqs_number << "\n";

			//clear vectors
			sources.clear();
			handlers.clear();
			reqs.clear();

			hm_proceeding_over.wait_for(hm_lock, std::chrono::milliseconds(3000));
			pm_proceeding_over.wait_for(pm_lock, std::chrono::milliseconds(3000));

			//reset counters and bools
			reqs_number = 0;
			handled_reqs_number = 0;
			existing_reqs_number = 0;
			handler_num = 0;
			source_num = 0;
			handlers_to_delete = 0;
			handle = true;
			handling_manager_proceed = true;
			placing_manager_proceed = true;
		}
	}
	return 0;
}