#include <catch2/catch_all.hpp>

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
        

// TODO: test_var does not sync across threads
// it remains [0] value, from time to time (confidently with valgrind)
// there is a need to let other threads know of updates on this part of memory [test_var]
// regardless of on which processor/core thread is executing
TEST_CASE("strand creation", "[!mayfail]") {
    const int num_threads = 5;
    const int num_tasks = 10;
    const int incr_num = 1000;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    std::atomic<int> test_var = 0;
    strand<io_service> test_strand(*srvc_ptr);
 
    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));


    auto incr_var_handle = 
        [incr_num, &test_var] () {
            // an attempt to sync test_var among threads
            for(int i = 0; i < incr_num; ++i)
                ++test_var;
        };

    for(int i = 0; i < num_tasks; ++i)
        test_strand.post(incr_var_handle);

    srvc_ptr->stop();

    // test_var: cache is not updated, reads initial value [0]
    REQUIRE(test_var == (num_tasks * incr_num));
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

TEST_CASE("dispatch into non-running strand") {
    const int num_threads = 10;
    const int num_strand_tasks = 100;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    typedef strand<io_service> handles_type;
    std::shared_ptr<handles_type> strand_ptr =
        std::make_shared<handles_type>(*srvc_ptr);

    // used for checking strand exclusivity (single thread executing)
    concurrency::mutex strand_mutex;

    std::atomic<bool> is_exclusive(true);
    std::atomic<bool> failed_dispatch(false);

    // dispatched into strand
    auto dispatched_task =
        [&strand_mutex, &is_exclusive, &failed_dispatch]
        <typename cntx_cntr_valid_Predicate>
        (
            std::weak_ptr<handles_type> handle_ptr,
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

            // increment dispatched task done
            if(counter)
                ++(*counter);
        };

    // posted to thread pool
    auto pool_task =
        [num_strand_tasks, &failed_dispatch, dispatched_task]
        <   typename dispatch_valid_Predicate,
            typename cntx_cntr_valid_Predicate>
        (
            std::weak_ptr<handles_type> handle_ptr,
            dispatch_valid_Predicate dispatch_pred,
            cntx_cntr_valid_Predicate cntx_pred
        ) {
            if(handle_ptr.expired())
                return;
            std::shared_ptr<handles_type> obj_ptr(handle_ptr);

            int counter = 0; /*count of tasks executed by dispatch*/
            callstack<handles_type, int>::context cntx(obj_ptr.get(), counter);

            for(int i = 0; i < num_strand_tasks; ++i)
                obj_ptr->dispatch(
                    std::bind(dispatched_task, handle_ptr, cntx_pred));

            if(!dispatch_pred(counter))
                failed_dispatch = true;
        };

    // post one task to thread pool, which will be the only runner of strand
    // so it can freely dispatch into strand and expect them to run within dispatch
    srvc_ptr->post(pool_task,
        std::weak_ptr<handles_type>(strand_ptr),
        /*validity of dispatcher task*/
        [num_strand_tasks] (int dispatched_tasks)
        { return dispatched_tasks == num_strand_tasks; },
        /*validity of dispatched task*/
        [] (int* cntx_cntr) { return cntx_cntr != nullptr; } );

    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));

    srvc_ptr->stop();

    REQUIRE(is_exclusive);
    REQUIRE(!failed_dispatch);
}

} // namespace io_service
