#include <catch2/catch_all.hpp>

#include <iostream>

#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#include <atomic>

#include "mutex.hpp"
#include "strand.hpp"
#include "io_service.hpp"

#include "jthread.hpp"
#include "shared_ptr.hpp"

namespace io_service {

void worker_thread(memory::shared_ptr<io_service> srvc_ptr)
    try { srvc_ptr->run(); }
    catch(service_stopped_error& e)
    { /*service stopped :-(*/ }
    catch(const std::runtime_error& e) {
        std::cerr << e.what() << '\n';
        REQUIRE(false); /*worker thread has unhandled exception*/
    }
        

// Original intent: test_var does not sync across threads
// it remains [0] value, from time to time (confidently with valgrind)
// there is a need to let other threads know of updates on this part of memory [test_var]
// regardless of on which processor/core thread is executing
TEST_CASE("strand creation", "[!mayfail]") {
    // Description: Original intent expected
    // that the strand will be executed continuously, yet it is not guaranteed
    // At some point, between pool tasks that call to run(), strand may be idle (not running)
    // and at that very moment, an io_service::stop() could be invoked,
    // thus not finishing remaining strand tasks
    const int num_threads = 5;
    const int num_tasks = 10;
    const int incr_num = 1000;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    int test_var = 0;
    std::atomic<int> total_increments(0);

    strand<io_service> test_strand(*srvc_ptr);
 
    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));


    auto incr_var_handle = 
        [incr_num, &test_var, &total_increments] () {
            // an attempt to sync test_var among threads
            for(int i = 0; i < incr_num; ++i)
                ++test_var;

            ++total_increments;
        };

    for(int i = 0; i < num_tasks; ++i)
        test_strand.post(incr_var_handle);

    srvc_ptr->stop();

    // test_var: cache is not updated, reads initial value [0]
    REQUIRE(test_var == (total_increments * incr_num));
}

template<typename Processor>
struct exclusive_data {
public:
    strand<Processor> handlers;
    concurrency::mutex exclusion;
public:
    exclusive_data(Processor& proc)
        : handlers(proc)
    {}

}; // struct exclusive_data

TEST_CASE("strand exclusive access") {
    const int num_threads = 10;
    const int num_tasks = 100;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));

    typedef exclusive_data<io_service> excl_type;
    std::shared_ptr<excl_type> obj_ptr =
            std::make_shared<excl_type>(*srvc_ptr);

    std::atomic<bool> is_exclusive(true);
    auto strand_job =
        [&is_exclusive] (std::weak_ptr<excl_type> excl_ptr) /*weak_ptr, so as to break cycle*/ {
            using namespace concurrency;

            if(excl_ptr.expired())
                return;

            std::shared_ptr<excl_type> obj_ptr = 
                excl_ptr.lock();
            // since no thread accesses excl_type at same time,
            // we can try_lock confidently
            if(!obj_ptr->exclusion.try_lock()) {
                is_exclusive = false;
                return; 
            }

            lock_guard<mutex> lk(obj_ptr->exclusion, adopt_lock); 
            std::this_thread::yield(); /*hold lock long enough*/
        };

     for(int i = 0; i < num_tasks; ++i)
        obj_ptr->handlers.post(
            std::bind(strand_job,
                /*bind stores args directly. need to convert to weak_ptr explicitly*/
                std::weak_ptr<excl_type>(obj_ptr))); 

    srvc_ptr->stop();

    REQUIRE(is_exclusive);
}

template<typename Processor>
struct exclusive_counter {
public:
    concurrency::recursive_mutex exclusion;
    int counter;
    strand<Processor> handlers;

    std::atomic<int> increments_total;

public:
    exclusive_counter(Processor& proc)
        : counter(0)
        , handlers(proc)
        , increments_total(0)
    {}
};

TEST_CASE("strand in running strand") {
    const int num_threads = 10;
    const int num_tasks = 100;
    const int num_increments = 100;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    typedef exclusive_counter<io_service> excl_cnt_type;
    std::shared_ptr<excl_cnt_type> obj1_ptr =
            std::make_shared<excl_cnt_type>(*srvc_ptr);

    std::shared_ptr<excl_cnt_type> obj2_ptr =
            std::make_shared<excl_cnt_type>(*srvc_ptr);

    std::atomic<bool> is_exclusive(true);
    std::atomic<bool> failed_dispatch(false);

    auto incr_task =
        [&is_exclusive, num_increments]
        (std::weak_ptr<excl_cnt_type> excl_ptr) /*weak_ptr, so as to break cycle*/ {
            using namespace concurrency;

            if(excl_ptr.expired())
                return;

            std::shared_ptr<excl_cnt_type> obj_ptr = 
                excl_ptr.lock(); /*get shared_ptr*/

            // exclusivity check
            if(!obj_ptr->exclusion.try_lock()) {
                is_exclusive = false;
                return; 
            }
            
            lock_guard<recursive_mutex> lk(obj_ptr->exclusion, adopt_lock); 
            // do increment job
            for(int i = 0; i < num_increments; ++i)
                ++obj_ptr->counter;

            ++obj_ptr->increments_total;
        };

    // worker thread, strand task which dispatches to own strand
    auto dispatch_own_task =
        [&is_exclusive, &failed_dispatch, num_increments, incr_task]
        (std::weak_ptr<excl_cnt_type> excl_ptr) /*weak_ptr, so as to break cycle*/ {
            using namespace concurrency;

            if(excl_ptr.expired())
                return;

            std::shared_ptr<excl_cnt_type> obj_ptr = 
                excl_ptr.lock(); /*get shared_ptr*/

            // exclusivity check
            if(!obj_ptr->exclusion.try_lock()) {
                is_exclusive = false;
                return; 
            }
            
            lock_guard<recursive_mutex> lk(obj_ptr->exclusion, adopt_lock); 
            // dispatch to own strand
            int capture_val = obj_ptr->counter;

            // guaranteed to execute exactly once
            obj_ptr->handlers.dispatch(std::bind(incr_task, excl_ptr));

            int cur_val = obj_ptr->counter;
            if(cur_val != (capture_val + num_increments)) {
                failed_dispatch = true;
            }
        };

    auto dispatch_tasks =
        [dispatch_own_task] (
            std::shared_ptr<excl_cnt_type>& own)
        {
            own->handlers.dispatch(
            std::bind(dispatch_own_task, std::weak_ptr<excl_cnt_type>(own)));
        };

    for(int i = 0; i < num_tasks; ++i) {
        dispatch_tasks(obj1_ptr);
        dispatch_tasks(obj2_ptr);
    }

    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));

    srvc_ptr->stop();

    REQUIRE(is_exclusive);
    REQUIRE(!failed_dispatch);
}

TEST_CASE("dispatch") {
    const int num_threads = 10;
    const int num_strand_tasks = 100;
    const int num_unlucky_dispatchers = 10;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    typedef strand<io_service> handles_type;
    std::shared_ptr<handles_type> strand_ptr =
        std::make_shared<handles_type>(*srvc_ptr);

    // used for checking strand exclusivity (single thread executing)
    concurrency::mutex strand_mutex;

    std::atomic<bool> is_exclusive(true);
    std::atomic<bool> failed_dispatch(false);

    std::vector<concurrency::jthread> trs;

    auto start_threads =
        [&trs, &srvc_ptr] () {
            for(int i = 0; i < num_threads; ++i)
                trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));
        };

    auto stop_and_test =
        [&] () {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);

            srvc_ptr->stop();
            REQUIRE(is_exclusive);
            REQUIRE(!failed_dispatch);
        };

    // dispatched into strand
    auto dispatched_task =
        [&strand_mutex, &is_exclusive, &failed_dispatch]
        <   typename strand_task_Callable,
            typename cntx_cntr_valid_Predicate>
        (
            std::weak_ptr<handles_type> handle_ptr,
            strand_task_Callable strand_task,
            cntx_cntr_valid_Predicate cntx_pred
        ) {
            if(handle_ptr.expired())
                return;

            // exclusivity check
            if(strand_mutex.try_lock() == false)
                is_exclusive = false;

            using namespace concurrency;
            lock_guard<mutex> lk(strand_mutex, adopt_lock);
            std::shared_ptr<handles_type> obj_ptr(handle_ptr);

            int* counter = 
                callstack<handles_type, int>::contains(obj_ptr.get());

            // if counter (i.e. callstack context) is not as expected
            if(!cntx_pred(counter)) {
                failed_dispatch = true;
                return;
            }

            // specific task, given to this strand handler
            strand_task();

            // increment dispatched task done
            if(counter)
                ++(*counter);
        };

    // posted to thread pool
    auto pool_task =
        [num_strand_tasks, &failed_dispatch, dispatched_task]
        <   typename worker_task_Callable,
            typename dispatch_valid_Predicate>
        (
            std::weak_ptr<handles_type> handle_ptr,
            worker_task_Callable worker_task,
            dispatch_valid_Predicate dispatch_pred
        ) {
            if(handle_ptr.expired())
                return;
            std::shared_ptr<handles_type> obj_ptr(handle_ptr);

            int counter = 0; /*count of tasks executed by dispatch*/
            callstack<handles_type, int>::context cntx(obj_ptr.get(), counter);

            // specific task, given to this thread pool worker
            worker_task(obj_ptr);
            
            if(!dispatch_pred(counter))
                failed_dispatch = true;
        };

    auto strand_empty_task =
        [] () {};

    auto worker_empty_task =
        [] (std::weak_ptr<handles_type> handle_ptr) {};

    SECTION("into non-running strand") {
        // post one task to thread pool, which will be the only runner of strand
        // so it can freely dispatch handlers into strand
        // and expect them to run within dispatch

        auto worker_task =
            [dispatched_task, strand_empty_task]
            (std::shared_ptr<handles_type>& obj_ptr) {
                std::weak_ptr<handles_type> handle_ptr(obj_ptr);

                for(int i = 0; i < num_strand_tasks; ++i)
                    obj_ptr->dispatch(
                        std::bind(dispatched_task, handle_ptr,
                            strand_empty_task,
                            /*validity of dispatched task*/
                            [] (int* cntx_cntr) { return cntx_cntr != nullptr; }));
            };

        srvc_ptr->post(pool_task,
            std::weak_ptr<handles_type>(strand_ptr),
            worker_task,
            /*validity of dispatcher task*/
            [num_strand_tasks] (int dispatched_tasks)
            { return dispatched_tasks == num_strand_tasks; });

        start_threads();
        stop_and_test();
    }

    SECTION("into running strand") {
        // Decription:
        // Since strand is being runned (by blocking task)
        // dispatch into this strand will not execute any task
        using namespace concurrency;
        mutex manual_block_mt;
        condition_variable manual_block_cv;
        bool manual_block = true; // block state

        auto block_until_notice =
            [&] () {
                unique_lock<mutex> lk(manual_block_mt);
                manual_block_cv.wait(lk, [&]() { return !manual_block; });
            };

        auto worker_blocker =
            [dispatched_task, block_until_notice] 
            (std::shared_ptr<handles_type>& obj_ptr) {
                std::weak_ptr<handles_type> handle_ptr(obj_ptr);

                for(int i = 0; i < num_strand_tasks; ++i)
                    obj_ptr->dispatch(
                        std::bind(dispatched_task, handle_ptr,
                            block_until_notice, /*blocking strand*/
                            /*this handler will be executed inside dispatch*/
                            [] (int* cntx_cntr) { return cntx_cntr != nullptr; }));
            };


        // post worker blocker
        srvc_ptr->post(pool_task,
            std::weak_ptr<handles_type>(strand_ptr),
            worker_blocker,
            /*validity of dispatcher task*/
            [] (int dispatched_tasks)
            { return dispatched_tasks >= 1/*strand blocker atleast + dispatched tasks*/; });

        auto unlucky_dispatcher =
            [dispatched_task, strand_empty_task]
            (std::shared_ptr<handles_type>& obj_ptr) {
                std::weak_ptr<handles_type> handle_ptr(obj_ptr);

                for(int i = 0; i < num_strand_tasks; ++i)
                    obj_ptr->dispatch(
                        std::bind(dispatched_task, handle_ptr,
                            strand_empty_task,
                            /*validity of dispatched task*/
                            [] (int* cntx_cntr) { return true; /*irrelevant*/ }));
            };

        // post non-executing dispatchers
        for(int i = 0; i < num_unlucky_dispatchers; ++i)
            srvc_ptr->post(pool_task,
                std::weak_ptr<handles_type>(strand_ptr),
                unlucky_dispatcher,
                /*validity of dispatcher task*/
                [&manual_block_mt, &manual_block, &failed_dispatch]
                (int dispatched_tasks)
                { 
                    lock_guard<mutex> lk(manual_block_mt);

                    // if test is over
                    if(manual_block == false)
                        return true;
 
                    return dispatched_tasks == 0/*no handlers executed*/;
                });

        start_threads();
        {
            unique_lock<mutex> lk(manual_block_mt);
            manual_block = false;
            manual_block_cv.notify_all();
        }
        stop_and_test();
    }

}

} // namespace io_service
