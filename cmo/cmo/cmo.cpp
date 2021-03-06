#include "stdafx.h"
#include "manager.h"
#include <vector>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <ctime>
#include <atomic>
#include <map>

using namespace std;

#define MIN_SOURCES_NUMBER 2
#define MAX_SOURCES_NUMBER 5
#define MIN_HANDLERS_NUMBER 1
#define MAX_HANDLERS_NUMBER 5
#define MIN_BUFFER_SIZE 1
#define MAX_BUFFER_SIZE 3
#define MAX_GENERATION_TIME 5000
#define MIN_GENERATION_TIME 3000
#define REQS_TO_GEN 10

vector<Source*> sources;
vector<Handler*> handlers;
deque<Request> reqs;
vector<Event> events;
mutex events_mutex;
mutex handlers_mutex;
mutex reqs_mutex;
//to sync handling operations
mutex handling_mutex[MAX_HANDLERS_NUMBER];
condition_variable handling_permission[MAX_HANDLERS_NUMBER];
bool there_is_req_to_handle;
Request *req_to_handle;
//to sync placing operations
mutex placing_mutex[MAX_SOURCES_NUMBER];
condition_variable placing_permission;
bool there_is_req_to_put;
Request *req_to_put;
mutex req_to_put_mutex;
bool placing_manager_is_free = false;

int buffer_size = 0;
int handlers_number = 0;

atomic<int> all_reqs = 0;
atomic<int> handled_reqs = 0;
atomic<int> existing_reqs = 0;

atomic<int> handlers_finished = 0;
atomic<int> sources_finished = 0;
bool pm_is_done = false;
bool hm_is_done = false;
bool statistic_is_done = false;

mutex placing_manager_status_mutex;

map<int, vector<Request>> existing_reqs_map; // key = source_number, value = requests vector

void source_func()
{
	Source *source = new Source(sources.size()); // create a source for every source_thread
	sources.push_back(source);
	existing_reqs_map.insert(pair<int, vector<Request>>(source->get_number(), *new vector<Request>));

	unique_lock<mutex> placing_lock(placing_mutex[source->get_number()]);
	//unique_lock<mutex> placing_lock(placing_mutex[0]);

	// block for random number generation
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(MIN_GENERATION_TIME, MAX_GENERATION_TIME);

	while (all_reqs < REQS_TO_GEN)
	{
		// simulate generation
		int random = dis(gen);
		this_thread::sleep_for(std::chrono::milliseconds(random / 10));
		Request &r = source->generate_request();

		events_mutex.lock();
		events.push_back(Event("Source " + to_string(source->get_number()) + " generated request at " + to_string(std::chrono::system_clock::to_time_t((r).get_generation_time())) + "\n"));
		events_mutex.unlock();

		all_reqs++;
		source->increment_generated_reqs(); // !!! does not increment on last generated request, that are > than REQ_TO_GEN, but declined_reqs increments for them
		placing_manager_status_mutex.lock();
		placing_permission.wait(placing_lock, [] {return placing_manager_is_free; });
		placing_manager_is_free = false;
		placing_manager_status_mutex.unlock();
		//placing_permission.wait(placing_lock);

		req_to_put_mutex.lock();
		req_to_put = &r;
		req_to_put_mutex.unlock();
		there_is_req_to_put = true;
		//this_thread::sleep_for(chrono::milliseconds(200));
		while (!placing_manager_is_free)
		{

		}
	}
	sources_finished.store(sources_finished + 1);

	while (!statistic_is_done)
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
			int source_num = r.get_source_number();
			std::chrono::system_clock::time_point gen_time = r.get_generation_time();

			std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

			// simulate handling
			int handle_time = (int)dis(gen) + MIN_GENERATION_TIME;
			events_mutex.lock();
			events.push_back(Event("Handler " + to_string(handler->get_number()) + " started to proceed\n")); // add time
			events_mutex.unlock();
			handler->handle(r, handle_time / 10);

			events_mutex.lock();
			events.push_back(Event("Handler " + to_string(handler->get_number()) + " finished proceeding on request " + to_string(source_num) + " " + to_string(std::chrono::system_clock::to_time_t(gen_time)) + "\n"));
			events_mutex.unlock();

			handled_reqs++;

			std::chrono::system_clock::time_point finish = std::chrono::system_clock::now();

			handler->set_uptime(finish - start);
			map<int, vector<Request>>::iterator it = existing_reqs_map.find(source_num);
			int counter = 0;
			for each (Request r_to_update in (*it).second)
			{
				if (r_to_update.get_generation_time() == gen_time)
				{
					(*it).second.at(counter).set_handling_time(finish - start);
					break;
				}
				counter++;
			}

			handlers_mutex.lock();
			handler->set_is_busy(false);
			handlers_mutex.unlock();
		}
	}
	handlers_finished.store(handlers_finished + 1);
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
			//Request *r = req_to_put;
			Request r = req_to_put->copy_req();
			req_to_put_mutex.unlock();
			there_is_req_to_put = false;

			reqs_mutex.lock();

			if (reqs.size() < buffer_size) // place request into buffer if it's not full
			{
				map<int, vector<Request>>::iterator it = existing_reqs_map.find((r).get_source_number());
				if (it != existing_reqs_map.end())
					(it->second).push_back(r);

				reqs.push_back(r);
				events_mutex.lock();
				events.push_back(Event("Put a request in buffer: source: " + to_string((r).get_source_number()) + ", time: " + to_string(std::chrono::system_clock::to_time_t((r).get_generation_time())) + "\n"));
				events_mutex.unlock();

				existing_reqs++;
			}
			else // delete request from buffer or do not place new request in it if buffer is full
			{
				Request req_to_remove = r;
				int i = 0;
				int curr_req = 0;
				for (Request req : reqs) // find request to remove (with highest source number)
				{
					if (req.get_source_number() > req_to_remove.get_source_number()) // if current req's source is higher than req_to_remove's then it's the new req_to_remove
					{
						req_to_remove = req;
						curr_req = i;
					}
					else if (req.get_source_number() == req_to_remove.get_source_number()) // if current req's source is equal to req_to_remove's source than compare generation time
					{
						if (req.get_generation_time() > req_to_remove.get_generation_time()) // if current req's gen time is higher then req_to_remove's then it's the new req_to_remove
						{
							req_to_remove = req;
							curr_req = i;
						}
					}
					i++;
				}

				// if initial request has to be removed, just ignore it
				if (req_to_remove.get_source_number() == r.get_source_number() && req_to_remove.get_generation_time() == r.get_generation_time())
				{
					events_mutex.lock();
					events.push_back(Event("Request is ignored: " + to_string((r).get_source_number()) + ", time: " + to_string(std::chrono::system_clock::to_time_t((r).get_generation_time())) + "\n")); \
						events_mutex.unlock();
				}
				else { // else if req_to_remove is not an initial request then remove it from buffer and place here a new request instead
					events_mutex.lock();
					events.push_back(Event("Request is removed: " + to_string(req_to_remove.get_source_number()) + ", time: " + to_string(std::chrono::system_clock::to_time_t(req_to_remove.get_generation_time())) + "\n"));
					events_mutex.unlock();
					reqs.erase(reqs.begin() + curr_req);
					map<int, vector<Request>>::iterator it = existing_reqs_map.find(req_to_remove.get_source_number());
					if (it != existing_reqs_map.end())
					{
						int counter = 0;
						for each (Request r_to_rmv in (*it).second)
						{
							if (r_to_rmv.get_generation_time() == req_to_remove.get_generation_time())
							{
								(*it).second.erase((*it).second.begin() + counter);
								break;
							}
							counter++;
						}
					}
					it = existing_reqs_map.find((r).get_source_number());
					if (it != existing_reqs_map.end())
						(it->second).push_back(r);
					reqs.push_back(r);

					events_mutex.lock();
					events.push_back(Event("Put a request in buffer: Source: " + to_string((r).get_source_number()) + ", time: " + to_string(std::chrono::system_clock::to_time_t((r).get_generation_time())) + "\n"));
					events_mutex.unlock();
				}
				sources.at(req_to_remove.get_source_number())->increment_declined_reqs();
			}
			reqs_mutex.unlock();
			placing_manager_is_free = true;
			placing_permission.notify_one();
			std::this_thread::sleep_for(std::chrono::milliseconds(70));
		}
	}
	pm_is_done = true;
	while (!statistic_is_done)
	{

	}
}

void handling_manager_func()
{
	vector<Handler*>::iterator current_handler = handlers.begin(); // iterator to iterate handlers vector

	while (!(reqs.empty() && all_reqs >= REQS_TO_GEN && pm_is_done))
	{
		reqs_mutex.lock();
		int times_to_do_loop = handlers_number; // make sure that none of existing handlers is free, then go on

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
				req_to_handle = &reqs.front();
				int source_num = req_to_handle->get_source_number();
				std::chrono::system_clock::time_point gen_time = req_to_handle->get_generation_time();
				map<int, vector<Request>>::iterator it = existing_reqs_map.find(source_num);
				int counter = 0;
				for each (Request r_to_update in (*it).second)
				{
					if (r_to_update.get_generation_time() == gen_time)
					{
						(*it).second.at(counter).set_buffer_spent_time(std::chrono::system_clock::now() - r_to_update.get_generation_time());
						break;
					}
					counter++;
				}
				reqs.pop_front(); // remove handling request from buffer
				there_is_req_to_handle = true;

				handling_permission[(*current_handler)->get_number()].notify_one(); // notify that free handler (but i'm not sure it works as intended??)
			}
			else
			{
			}
			handlers_mutex.unlock();
		}
		else
		{
		}
		reqs_mutex.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	hm_is_done = true;
}


int main()
{
	vector<thread*> source_threads;
	vector<thread*> handler_threads;

	srand((int)&handlers);

	for (int srcs = 5; srcs <= MAX_SOURCES_NUMBER; srcs++)
	{
		for (handlers_number = 1; handlers_number <= MAX_HANDLERS_NUMBER; handlers_number++)
		{
			for (buffer_size = 1; buffer_size <= MAX_BUFFER_SIZE; buffer_size++)
			{
				for (int i = 0; i < srcs; i++) // create source threads
				{
					thread *t = new thread(source_func);
					source_threads.push_back(t);
					t->detach();
				}
				this_thread::sleep_for(chrono::milliseconds(1000));
				thread(placing_manager_func).detach();
				//this_thread::sleep_for(chrono::milliseconds(5000));
				for (int i = 0; i < handlers_number; i++) // create source threads
				{
					thread *t = new thread(handler_func);
					handler_threads.push_back(t);
					t->detach();
				}
				thread(handling_manager_func).detach();

				cout << "SOURCES: " << srcs << ", HANDLERS: " << handlers_number << ", BUF SIZE: " << buffer_size << "\n";
				std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

				while (handlers_finished < handlers.size())
				{
				}

				std::chrono::system_clock::time_point finish = std::chrono::system_clock::now();
				std::chrono::duration<double> est_time = finish - start;
				cout << "Estimated time: " << est_time.count() << "\n";
				cout << "AR = " << all_reqs << "; HR = " << handled_reqs << "; ER = " << existing_reqs << "\n";

				for each (Handler *h in handlers)
				{
					std::chrono::duration<double> h_upt = h->get_uptime();
					cout << "Handler's " << h->get_number() << " uptime = " << h_upt.count() << ". Efficiency = " << (double)h_upt.count() / est_time.count() << "\n";
				}

				cout << "==========================================================================================\n";

				double common_decline_rate = 0;

				for each (pair<int, vector<Request>> pair in existing_reqs_map)
				{
					int vec_size = pair.second.size();
					int source_generated_reqs = sources[pair.first]->get_generated_reqs();
					int source_declined_reqs = sources[pair.first]->get_declined_reqs();
					double source_decline_rate_percent = 0;
					if (source_declined_reqs != 0)
						source_decline_rate_percent = (double)source_declined_reqs / source_generated_reqs * 100;
					common_decline_rate += source_decline_rate_percent;

					cout << "Source " << pair.first << ". Reqs: " << source_generated_reqs << ", declined reqs: " << source_declined_reqs << ", decline rate: " << source_decline_rate_percent << "\%\n";
					if (vec_size > 0)
					{
						std::chrono::duration<double> common_buf_time = std::chrono::duration<double>::zero();
						std::chrono::duration<double> common_hndl_time = std::chrono::duration<double>::zero();
						for each (Request request in pair.second)
						{
							common_buf_time += request.get_buffer_spent_time();
							common_hndl_time += request.get_handling_time();
						}

						std::chrono::duration<double> buf_time_expected_value = common_buf_time / vec_size;
						std::chrono::duration<double> hndl_time_expected_value = common_hndl_time / vec_size;
						std::chrono::duration<double> full_time_in_system_expected_value = buf_time_expected_value + hndl_time_expected_value;

						cout << "Buf mean time: " << buf_time_expected_value.count() << ", handling mean time: " << hndl_time_expected_value.count() << ", full time: " << full_time_in_system_expected_value.count() << "\n";

						double buf_time_dispersion = 0;
						double hndl_time_dispersion = 0;
						for each (Request request in pair.second)
						{
							buf_time_dispersion += pow((request.get_buffer_spent_time().count() - buf_time_expected_value.count()), 2);
							hndl_time_dispersion += pow((request.get_handling_time().count() - hndl_time_expected_value.count()), 2);
						}

						if (vec_size - 1 > 0)
						{
							buf_time_dispersion /= vec_size - 1;
							hndl_time_dispersion /= vec_size - 1;
							cout << "Buf time dispersion: " << buf_time_dispersion << ", handling time dispersion: " << hndl_time_dispersion << "\n";
						}
						else
							cout << "Need more elements to make dispersion statistic\n";

					}
					cout << "==========================================================================================\n";
				}
				common_decline_rate /= srcs;
				cout << "Common decline rate: " << common_decline_rate << "\%\n";
				cout << "==========================================================================================\n";

				statistic_is_done = true;

				this_thread::sleep_for(chrono::milliseconds(2000));

				handlers_finished = 0;
				sources_finished = 0;
				all_reqs = 0;
				handled_reqs = 0;
				existing_reqs = 0;
				sources.clear();
				handlers.clear();
				reqs.clear();
				existing_reqs_map.clear();
				hm_is_done = false;
				pm_is_done = false;
				statistic_is_done = false;
				there_is_req_to_put = false;

				this_thread::sleep_for(chrono::milliseconds(1000));
			}
		}
	}
	return 0;
}