#ifndef ASIO_THREADSAFE_QUEUE_HPP
#define ASIO_THREADSAFE_QUEUE_HPP

#include "helgrind_annotations.hpp"

#include "mutex.hpp"
#include "condition_variable.hpp"
#include "lock_guard.hpp"
#include "unique_lock.hpp"

#include <memory> 
#include <mutex> // std::scoped_lock

namespace io_service::new_impl {

template<typename T>
class threadsafe_queue {
private:
	struct node {
		T data;
		std::unique_ptr<node> next_node;
	};

private:
	std::unique_ptr<node> m_head;
	node* m_tail;

	concurrency::mutex m_head_mutex;
	concurrency::mutex m_tail_mutex;
	concurrency::condition_variable m_data_cv;
	
private:
	// TODO: Consider adding copying, depending on T
	threadsafe_queue(const threadsafe_queue& other) = delete;
	threadsafe_queue& operator=(const threadsafe_queue& other) = delete;

public:
	threadsafe_queue()
		: m_head(std::make_unique<node>()) /*dummy node*/
		, m_tail(m_head.get())
	{}

	threadsafe_queue(threadsafe_queue&& other) {
		std::scoped_lock lk(
			m_head_mutex, m_tail_mutex,
			other.m_head_mutex, other.m_tail_mutex);

		m_head = std::move(other.m_head);
		m_tail = other.m_tail;
	}

public:

	void push(T in_data) {
		using namespace concurrency;

		/* new dummy node */
		std::unique_ptr<node> new_node_ptr =
			std::make_unique<node>();
		node* new_tail = new_node_ptr.get();

		{
			lock_guard<mutex> lk(m_tail_mutex);
			m_tail->data = std::move(in_data);
			m_tail->next_node = std::move(new_node_ptr);
			m_tail = new_tail;
		}	

		// TODO: is it fine to notify outside of any lock?
		m_data_cv.notify_one();
	}

	bool try_pop(T& out_data) {
		using namespace concurrency;
		
		lock_guard<mutex> lk(m_head_mutex);
		if(m_head.get() == m_tail)
			return false;

		do_pop_head(out_data);
		return true;
	}

	void wait_and_pop(T& out_data) {
		using namespace concurrency;

		unique_lock<mutex> lk(wait_for_data());

		do_pop_head(out_data);
	}

	bool empty() {
		using namespace concurrency;
		lock_guard<mutex> lk(m_head_mutex);
		return m_head.get() == get_tail();
	}

// Impl funcs
private:
	node* get_tail() {
		using namespace concurrency;
		lock_guard<mutex> lk(m_tail_mutex);
		return m_tail;
	}

	concurrency::unique_lock<concurrency::mutex>
	wait_for_data() {
		using namespace concurrency;
		unique_lock<mutex> lk(m_head_mutex);
		m_data_cv.wait(lk,
				[this] () { return m_head.get() != get_tail(); });
		return lk;
	}

	// Prereq: head_mutex - locked
	void
	do_pop_head(T& out_data) {
		out_data = std::move(m_head->data);
		std::unique_ptr<node> old_head = std::move(m_head);
		m_head = std::move(old_head->next_node);
	}

};

} // namespace io_service::new_impl


#endif
