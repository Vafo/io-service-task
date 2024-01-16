#ifndef ASIO_INVOCABLE_HPP
#define ASIO_INVOCABLE_HPP

#include <future>
#include <memory>
#include <type_traits>

namespace io_service::new_impl {

namespace detail {

template<typename Callable, typename ...Args>
std::packaged_task<std::result_of_t<Callable()>()>
pack_task_and_args(Callable calb, Args... args) {
	typedef std::result_of_t<Callable()> return_type;
	std::packaged_task<return_type()> new_task(
		// store args in lambda
		[calb, args...]() -> return_type {
			return calb(args...);
		});

	return new_task;
}

}; // namespace detail

struct invocable_int {
	virtual ~invocable_int() {}

	virtual void call() = 0;
}; // struct invocable_int


template<typename Callable>
struct invocable_impl: public invocable_int {
public:
	// typedef std::result_of_t<Callable> return_type;
	typedef std::packaged_task<Callable> task_type;

private:
	task_type m_task;

private:
	invocable_impl(const invocable_impl& other) = delete;
	invocable_impl& operator=(const invocable_impl& other) = delete;

public:
	invocable_impl(task_type&& task)
		: m_task(std::move(task))
	{}
	
	void call() {
		m_task();
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
	template<typename Callable>
	invocable(std::packaged_task<Callable>&& task)
		: m_inv_ptr( 
			std::make_unique<invocable_impl<Callable>>(
				std::move(task)))
	{}

public:
	void operator()() {
		if(m_inv_ptr)
			m_inv_ptr->call();
	}

public:
	void swap(invocable& other) {
		using std::swap;
		swap(m_inv_ptr, other.m_inv_ptr);
	}

	void swap(invocable& a, invocable& b)
	{ a.swap(b); }

}; // struct invocable


template<typename Callable, typename ...Args>
invocable make_invocable(Callable calb, Args... args) {
	typedef std::result_of_t<Callable> return_type;
	std::packaged_task<return_type()> new_task(
		detail::pack_task_and_args(calb, args...));

	return invocable(std::move(new_task));
}

template<typename Callable, typename ...Args>
invocable make_invocable(
	std::future<std::result_of_t<Callable>>& fut,
	Callable calb,
	Args... args
) {
	typedef std::result_of_t<Callable> return_type;
	std::packaged_task<return_type()> new_task(
		detail::pack_task_and_args(calb, args...));

	// obtain future of task
	fut = new_task.get_future();
	return invocable(std::move(new_task));
}

} // namespace io_service::new_impl

#endif // ASIO_INVOCABLE_HPP
