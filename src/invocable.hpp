#ifndef ASIO_INVOCABLE_HPP
#define ASIO_INVOCABLE_HPP

#include "helgrind_annotations.hpp"

#include <future>
#include <memory>
#include <tuple>

namespace io_service::new_impl {
struct invocable_int {
	virtual ~invocable_int() {}

	virtual void call() = 0;
}; // struct invocable_int


template<typename Callable, typename TupleT>
struct invocable_impl: public invocable_int {
public:
	// typedef std::result_of_t<Callable> return_type;
	typedef std::packaged_task<Callable> task_type;

private:
	task_type m_task;
	TupleT m_args;

private:
	invocable_impl(const invocable_impl& other) = delete;
	invocable_impl& operator=(const invocable_impl& other) = delete;

public:
	invocable_impl(task_type&& task, TupleT&& args)
		: m_task(std::move(task))
		, m_args(args)
	{}
	
	void call() {
		std::apply(m_task, m_args);
	}

}; // struct invocable_impl


// Type Erasure of packaged_task
struct invocable {
private:
	std::unique_ptr<invocable_int> m_inv_ptr;

private:
	invocable(const invocable& other) = delete;
	invocable& operator=(const invocable& other) = delete;

public:
	invocable()
		: m_inv_ptr()
	{}

	invocable(invocable&& other)
		: m_inv_ptr(other.m_inv_ptr.release())
	{}

	invocable& operator=(invocable&& other) {
		invocable(std::move(other)).swap(*this);
		return *this;
	}

public:
	// TODO: Simplify interface.
	// Let user pass packaged task and args
	template<typename Callable, typename ...Args>
	invocable(
		std::packaged_task<Callable>&& task,
		Args... args
	)
		: m_inv_ptr( 
			std::make_unique<
				invocable_impl<Callable, std::tuple<Args...>>>(
					std::move(task), std::make_tuple(args...)))
	{}

public:
	void operator()() {
		if(m_inv_ptr)
			m_inv_ptr->call();

		m_inv_ptr.reset();
	}

public:
	void swap(invocable& other) {
		using std::swap;
		swap(m_inv_ptr, other.m_inv_ptr);
	}

	void swap(invocable& a, invocable& b)
	{ a.swap(b); }

}; // struct invocable


} // namespace io_service::new_impl

#endif // ASIO_INVOCABLE_HPP
