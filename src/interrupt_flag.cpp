#include "interrupt_flag.hpp"

#include "lock_guard.hpp"
#include <memory>
#include <stdexcept>


namespace io_service::new_impl {

void interrupt_flag::wait_all() {
	std::shared_ptr<int> a;
	using namespace concurrency;
	unique_lock<mutex> lk(m_stop_signal_mutex);
	m_stop_signal_cv.wait(lk, [this] () { return m_counter == 0; });
}

void interrupt_flag::stop_all()
{ m_done = true; }

void interrupt_flag::incr() {
	if(m_done)
		// TODO: Should handle throw exception?
		// Or just count up, and decrement on destruction?
		// Will it bother with stopped thread pool,
		// causing it to wait infinitely?
		throw std::logic_error(
			"thread pool is stopped. Cant add new thread");

	++m_counter;
}

void interrupt_flag::decr() {
	if(m_counter == 0)
		return;

	if(--m_counter == 0) {
		using namespace concurrency;
		lock_guard<mutex> lk(m_stop_signal_mutex);
		m_stop_signal_cv.notify_one();
	}
}

bool interrupt_flag::is_stopped()
{ return m_done; }


} // namespace io_service::new_impl


