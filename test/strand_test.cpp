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
struct exclusive_counter
    : public exclusive_data<Processor> {
public:
    int counter;

public:
    exclusive_counter(Processor& proc)
        : exclusive_data<Processor>(proc)
    {}
};

TEST_CASE("strand dispatch") {
    const int num_threads = 10;
    const int num_tasks = 100;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));

    typedef exclusive_counter<io_service> excl_cnt_type;
    std::shared_ptr<excl_cnt_type> obj_ptr =
            std::make_shared<excl_cnt_type>(*srvc_ptr);

    std::atomic<bool> is_exclusive;

    auto incr_task =
        [&is_exclusive] (std::weak_ptr<excl_cnt_type> excl_ptr) /*weak_ptr, so as to break cycle*/ {
            using namespace concurrency;

            if(excl_ptr.expired())
                return;

            std::shared_ptr<excl_cnt_type> obj_ptr = 
                excl_ptr.lock(); /*get shared_ptr*/

            // since no thread accesses excl_type at same time,
            // we can try_lock confidently
            // yet redundant, as no other thread takes part, but at least syncs data among threads
            if(!obj_ptr->exclusion.try_lock()) {
                is_exclusive = false;
                return; 
            }
            
            lock_guard<mutex> lk(obj_ptr->exclusion, adopt_lock); 

            // dispatch smth
        };

    srvc_ptr->stop();
}

} // namespace io_service
