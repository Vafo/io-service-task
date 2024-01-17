#include <catch2/catch_all.hpp>

#include "thread_manager.hpp"

#include <thread> // std::this_thread::yield()
#include <atomic>
#include "jthread.hpp"


namespace io_service::new_impl {

TEST_CASE("thread_manager creation", "[thread_manager]") {
	thread_manager manager;
	// It is safe by design to decrement empty manager
	REQUIRE_NOTHROW(manager.decr());

	REQUIRE_NOTHROW(manager.incr());
	REQUIRE_NOTHROW(manager.decr());
	REQUIRE_NOTHROW(manager.decr());

	REQUIRE(manager.is_stopped() == false);
	manager.stop_all();
	REQUIRE(manager.is_stopped() == true);

	REQUIRE_THROWS(manager.incr());
	// It is safe to decrement stopped manager
	REQUIRE_NOTHROW(manager.decr());
}

TEST_CASE("thread_manager stopping", "[thread_manager]") {
	int const threads_num = 10;

	thread_manager manager;
	std::atomic<int> threads_entered(0);
	std::atomic<int> threads_stopped(0);

	using namespace concurrency;
	std::vector<jthread> threads;
	for(int i = 0; i < threads_num; ++i)
		threads.push_back( jthread(
			[&manager, &threads_entered, &threads_stopped] () {
				thread_handle handle(manager);
				++threads_entered;
				while(!manager.is_stopped())
					std::this_thread::yield();	
				++threads_stopped;
			}));

	// busy loop until all threads pass thread handle cstr
	// otherwise, exception will be thrown
	while(threads_entered != threads_num)
		std::this_thread::yield();

	// cause threads to stop
	manager.stop_all();
	manager.wait_all();
	REQUIRE(threads_stopped == threads_num);
}

} // namespace io_service::new_impl

