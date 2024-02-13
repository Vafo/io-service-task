#include <catch2/catch_all.hpp>

#include "interrupt_flag.hpp"

#include <thread> // std::this_thread::yield()
#include <atomic>
#include "jthread.hpp"


namespace io_service {

TEST_CASE("interrupt_flag creation", "[interrupt_flag]") {
    interrupt_flag manager;

    // No one to wait for, return immediately
    REQUIRE_NOTHROW(manager.wait_all());

    {
        interrupt_handle handle = manager.make_handle();
        REQUIRE(manager.owns(handle));
        REQUIRE(!handle.is_stopped());
        REQUIRE(!manager.is_stopped());

        REQUIRE(manager.is_stopped() == false);
        manager.signal_stop();
        REQUIRE(manager.is_stopped() == true);
    }

    // No one to wait for, return immediately
    REQUIRE_NOTHROW(manager.wait_all());
    REQUIRE_NOTHROW(manager.signal_stop());

    interrupt_handle handle = manager.make_handle();
    REQUIRE(!manager.owns(handle));

    // No one to wait for, return immediately
    REQUIRE_NOTHROW(manager.wait_all());
}

TEST_CASE("interrupt_flag stopping", "[interrupt_flag]") {
    int const threads_num = 10;

    interrupt_flag manager;
    std::atomic<int> threads_entered(0);
    std::atomic<int> threads_stopped(0);

    using namespace concurrency;
    std::vector<jthread> threads;
    for(int i = 0; i < threads_num; ++i)
        threads.push_back( jthread(
            [&manager, &threads_entered, &threads_stopped] () {
                interrupt_handle handle = manager.make_handle();

                if(handle.is_stopped())
                    return;

                ++threads_entered;
                while(!handle.is_stopped()) {
                    std::this_thread::yield();
                }
                ++threads_stopped;
            }));
/*
    // busy loop until all threads pass thread handle cstr
    // otherwise, exception will be thrown
    while(threads_entered != threads_num)
        std::this_thread::yield();
*/
    // cause threads to stop
    manager.signal_stop();
    manager.wait_all();
    REQUIRE(threads_stopped == threads_entered);
}

} // namespace io_service

