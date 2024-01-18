#ifndef ASIO_THREAD_MANAGER_HPP
#define ASIO_THREAD_MANAGER_HPP

#include "mutex.hpp"
#include "condition_variable.hpp"

#include <atomic>
#include <memory>


namespace io_service::new_impl {

class interrupt_flag {
private:
	std::atomic<bool> m_done;
	std::atomic<int> m_counter; // of threads

	// Used to hold io_service waiting for workers to finish
    concurrency::mutex m_stop_signal_mutex;
    concurrency::condition_variable m_stop_signal_cv;

private:
	interrupt_flag(const interrupt_flag& other) = delete;
	interrupt_flag& operator=(const interrupt_flag& other) = delete;

public:
	interrupt_flag()
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

}; // class interrupt_flag


// RAII increment / decrement thread counter
class interrupt_handle {
private:
	// TODO: Finalize matter on safety of references
	interrupt_flag& m_manager_ref;

private:
	interrupt_handle(const interrupt_handle& other) = delete;
	interrupt_handle& operator=(const interrupt_handle& other) = delete;

public:
	interrupt_handle(interrupt_flag& manager)
		: m_manager_ref(manager)
	{ m_manager_ref.incr(); }

	~interrupt_handle()
	{ m_manager_ref.decr(); }

}; // interrupt_handle

} // namespace io_service::new_impl

#endif
