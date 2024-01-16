#ifndef ASIO_THREADSAFE_QUEUE_HPP
#define ASIO_THREADSAFE_QUEUE_HPP

#include "mutex.hpp"
#include "condition_variable.hpp"
#include "lock_guard.hpp"
#include "unique_lock.hpp"

#include <memory> 

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
	
public:
	threadsafe_queue()
		: m_head(std::make_unique<node>()) /*dummy node*/
		, m_tail(m_head.get())
	{}

public:

	void push(T in_data) {
		using namespace concurrency;

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

		out_data = std::move(m_head->data);
		std::unique_ptr<node> old_head = std::move(m_head);
		m_head = std::move(old_head->next_node);
	}

	void wait_and_pop(T& out_data) {
		using namespace concurrency;

		unique_lock<mutex> lk(m_head_mutex);
		m_data_cv.wait(lk,
				[this] () { return m_head.get() != get_tail(); });

		out_data = std::move(m_head->data);
		std::unique_ptr<node> old_head = std::move(m_head);
		m_head = std::move(old_head->next_node);
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

};

} // namespace io_service::new_impl


#endif
