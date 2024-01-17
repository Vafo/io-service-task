#ifndef ASIO_THREAD_MANAGER_HPP
#define ASIO_THREAD_MANAGER_HPP

#include "mutex.hpp"
#include "condition_variable.hpp"

#include <atomic>


namespace io_service::new_impl {

class thread_manager {
private:
	std::atomic<bool> m_done;
	std::atomic<int> m_counter; // of threads

	// Used to hold io_service waiting for workers to finish
    concurrency::mutex m_stop_signal_mutex;
    concurrency::condition_variable m_stop_signal_cv;

private:
	thread_manager(const thread_manager& other) = delete;
	thread_manager& operator=(const thread_manager& other) = delete;

public:
	thread_manager()
		: m_done(false)
		, m_counter(0)
	{}

public:
	// Wait for all threads to finish
	void wait_all(); 

	void stop_all();

public:
	void incr(); 

	void decr();

	bool is_stopped();

}; // class thread_manager


// RAII increment / decrement thread counter
class thread_handle {
private:
	// TODO: Finalize matter on safety of references
	thread_manager& m_manager_ref;

private:
	thread_handle(const thread_handle& other) = delete;
	thread_handle& operator=(const thread_handle& other) = delete;

public:
	thread_handle(thread_manager& manager)
		: m_manager_ref(manager)
	{ m_manager_ref.incr(); }

	~thread_handle()
	{ m_manager_ref.decr(); }

}; // thread_handle

} // namespace io_service::new_impl

#endif
