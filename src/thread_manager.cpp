#include "thread_manager.hpp"

#include "lock_guard.hpp"


namespace io_service::new_impl {

void thread_manager::wait_all() {
	using namespace concurrency;
	unique_lock<mutex> lk(m_stop_signal_mutex);
	m_stop_signal_cv.wait(lk);
}

void thread_manager::stop_all()
{ m_done = true; }

void thread_manager::incr() {
	if(m_done)
		throw std::runtime_error(
			"thread pool is stopped. Cant add new thread");

	++m_counter;
}

void thread_manager::decr() {
	if(m_counter == 0) {
		using namespace concurrency;
		lock_guard<mutex> lk(m_stop_signal_mutex);
		m_stop_signal_cv.notify_one();

		return;
	}

	--m_counter;
}

bool thread_manager::is_stopped()
{ return m_done; }


} // namespace io_service::new_impl


