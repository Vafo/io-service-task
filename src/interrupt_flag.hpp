#ifndef ASIO_THREAD_MANAGER_HPP
#define ASIO_THREAD_MANAGER_HPP

// #include "lock_guard.hpp"
#include "function.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"

#include <atomic>
#include <stdexcept>
#include <vector>

namespace io_service {

namespace detail {

class int_state_cb {
public:
	typedef func::function<void()> stop_cb_type;

private:
	std::atomic<bool> m_done;
	// Number of "owners". Manager + pool threads
	// This counter should go down to 1, so that Manager can be sure that everyone stopped
	std::atomic<int> m_owner_cnt;

	// Counter for paused threads can be added, when Manager decides to pause the pool
	// in which case, threads are not executing tasks, but waiting for Manager to start again

	// Used to hold Manager waiting for workers to finish
    concurrency::mutex m_stop_signal_mutex;
    concurrency::condition_variable m_stop_signal_cv;

	// No need to lock, since will only be called by interrupt_flag, the only owner
	std::vector<stop_cb_type> stop_cbs;

public: /*maybe private?*/
	int_state_cb()
		: m_done(false)
		, m_owner_cnt(1)
	{}

public:
	void do_wait() {
		using namespace concurrency;
		unique_lock<mutex> lk(m_stop_signal_mutex);
		m_stop_signal_cv.wait(lk, [this] () { return m_owner_cnt == 1; });
	}

	void do_stop()
	{
		m_done = true;
		for(std::vector<stop_cb_type>::iterator it = stop_cbs.begin();
			it != stop_cbs.end(); ++it
		)
			(*it)();
	}

	void add_stop_cb(func::function<void()>&& cb_fn)
	{ stop_cbs.push_back(cb_fn); }

	bool is_stopped() const
	{ return m_done; }


public:
	bool incr_own() {
		// When State is stopped, then no new handle can be added.
		// (Consider if exception should be thrown)
		// TODO: It would be much simpler just to throw exception
		if(is_stopped())
			return false;

		++m_owner_cnt;
		return true;
	}

	// Called only by threads, which are among "owners"
	void decr_own() {
		int cur_cnt = --m_owner_cnt;	
		if(cur_cnt == 1) {
			// Notify Manager
			using namespace concurrency;
			// Redundant to lock on signaling
			// lock_guard<mutex> lk(m_stop_signal_mutex);
			m_stop_signal_cv.notify_one();
		} else if(cur_cnt == 0) {
			// delete itself
			delete this;
		}
	}

}; // class int_state_cb

} // namespace detail


// shared_ptr like state
// that notifies cond_var every time there is only one reference
class int_state {
private:
	detail::int_state_cb* m_cb_ptr;

private:
	// special cstr for interrupt_flag
	int_state(detail::int_state_cb* new_ptr)
		: m_cb_ptr(new_ptr)
	{}

public:
	int_state()
		: m_cb_ptr(nullptr)
	{}

	int_state(const int_state& other)
		: m_cb_ptr(other.m_cb_ptr)
	{
		bool is_in_thread_pool = false;
		if(!empty())
			is_in_thread_pool = m_cb_ptr->incr_own();

		// State is stopped, could not become owner
		// Forget about it
		if(!is_in_thread_pool)
			m_cb_ptr = nullptr;
	}

	int_state(int_state&& other)
		: m_cb_ptr(other.m_cb_ptr)
	{ 
		other.m_cb_ptr = nullptr;
	}

	int_state& operator=(int_state other) {
		swap(other);
		return *this;
	}

	~int_state() {
		if(!empty())
			// Called only when in thread pool, so as to prevent late comers
			m_cb_ptr->decr_own();
	}

public:
	bool is_stopped() const
	{ 
		if(!empty())
			return m_cb_ptr->is_stopped();

		// if no state associated, it is stopped
		return true;
	}

	bool empty() const
	{ return m_cb_ptr == nullptr; }

private:
	// interrupt_flag only interface
	friend class interrupt_flag;

	static int_state S_make_int_state()
	{ return int_state(new detail::int_state_cb());  }

	void do_stop() {
		if(empty())
			throw std::runtime_error("State is empty");
		m_cb_ptr->do_stop();
	}

	void do_wait() {
		if(empty())
			throw std::runtime_error("State is empty");
		m_cb_ptr->do_wait();
	}

	void add_stop_cb(func::function<void()>&& cb_fn) {
		if(empty())
			throw std::runtime_error("State is empty");
		m_cb_ptr->add_stop_cb(
			std::forward<func::function<void()>>(cb_fn));
	}

public:
	bool operator==(const int_state& other) const
	{ return m_cb_ptr == other.m_cb_ptr; }

public:
	void swap(int_state& other) {
		using std::swap;
		swap(m_cb_ptr, other.m_cb_ptr);
	}

	friend
	void swap(int_state& a, int_state& b)
	{ a.swap(b); }

}; // class int_state


// Handle to interrupt source (flag)
// used by worker thread
class interrupt_handle {
private:
	int_state m_state;

private:
	interrupt_handle() = delete; /*created only by interrupt_flag*/
	interrupt_handle(const interrupt_handle& other) = delete;
	interrupt_handle& operator=(const interrupt_handle& other) = delete;

private:
	// Used by interrupt_flag
	interrupt_handle(int_state state)
		: m_state(state) 
	{}

public:
	interrupt_handle(interrupt_handle&& other)
		: m_state(std::move(other.m_state))
	{}

	interrupt_handle& operator=(interrupt_handle&& other)
	{ 
		m_state = std::move(other.m_state);
		return *this;
	}

public:
	bool is_stopped() const
	{ return m_state.is_stopped(); }

	bool empty() const
	{ return m_state.empty(); }
	
public:
	bool operator==(const interrupt_handle& other)
	{ return m_state == other.m_state; }

private:
	friend class interrupt_flag;

}; // interrupt_handle

// pool thread interrupt source.
// used by pool
class interrupt_flag {
private:
	int_state m_state;

private:
	interrupt_flag(const interrupt_flag& other) = delete;
	interrupt_flag& operator=(const interrupt_flag& other) = delete;

public:
	interrupt_flag()
		: m_state(int_state::S_make_int_state())
	{}

	interrupt_flag(interrupt_flag&& other)
		: m_state(std::move(other.m_state))
	{}

	interrupt_flag& operator=(interrupt_flag&& other) {
		interrupt_flag(std::move(other)).swap(*this);
		return *this;
	}

	~interrupt_flag()
	{ signal_stop(); /*let everyone free*/ }

public:
	// Wait for all threads to finish
	void wait_all()
	{ m_state.do_wait(); }

	void signal_stop()
	{ m_state.do_stop(); }

	bool is_stopped() const
	{ return m_state.is_stopped(); }

public:
	interrupt_handle make_handle()
	{ return interrupt_handle(m_state); }

	bool owns(const interrupt_handle& handle) const
	{ return m_state == handle.m_state; }

	// Callbacks to be called when Manager sets stop state
	void add_callback_on_stop(func::function<void()> cb) {
		m_state.add_stop_cb(std::move(cb));	
	}

public:
	void swap(interrupt_flag& other) {
		using std::swap;
		swap(m_state, other.m_state);
	}

	friend void swap(interrupt_flag& a, interrupt_flag&b)
	{ a.swap(b); }

}; // class interrupt_flag

} // namespace io_service

#endif
