#include "thread_manager.hpp"

#include "lock_guard.hpp"
#include <stdexcept>


namespace io_service::new_impl {

void thread_manager::wait_all() {
	using namespace concurrency;
	unique_lock<mutex> lk(m_stop_signal_mutex);
	m_stop_signal_cv.wait(lk, [this] () { return m_counter == 0; });
}

void thread_manager::stop_all()
{ m_done = true; }

void thread_manager::incr() {
	if(m_done)
		// TODO: Should handle throw exception?
		// Or just count up, and decrement on destruction?
		// Will it bother with stopped thread pool,
		// causing it to wait infinitely?
		throw std::logic_error(
			"thread pool is stopped. Cant add new thread");

	++m_counter;
}

void thread_manager::decr() {
	if(m_counter == 0)
		return;

	if(--m_counter == 0) {
		using namespace concurrency;
		lock_guard<mutex> lk(m_stop_signal_mutex);
		m_stop_signal_cv.notify_one();
	}
}

bool thread_manager::is_stopped()
{ return m_done; }


} // namespace io_service::new_impl


