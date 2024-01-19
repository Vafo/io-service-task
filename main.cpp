#include <iostream>

#include <vector>

#include "io_service.hpp"
#include "jthread.hpp"

#include <thread> // this_thread::sleep_for
#include <chrono> // chrono_literals

void worker_func(io_service::io_service* service_ptr) {
    service_ptr->run();
}

int main(int argc, char* argv[]) {
    using namespace concurrency;
    using io_service::io_service;

    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 20;
    
    int a = 0;
    std::atomic<int> tasks_complete = 0;
    concurrency::recursive_mutex a_mutex;

    /*Tasks definition*/

    // counting task
    auto counting_task = 
        [&a, &a_mutex, num_iterations, &tasks_complete] () {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            for(int i = 0; i < num_iterations; ++i)
                a += 1;
            
            tasks_complete++;
        };

    auto dispatch_task_local =
        [&a, &a_mutex, num_iterations, counting_task] (io_service* serv_ptr) {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            int cur_val = a;
            // If executes right now, a_mutex will recursively lock, and thus, proceed
            serv_ptr->dispatch(counting_task);
        };
    
    auto dispatch_task_foreign =
        [&a, &a_mutex, num_iterations, counting_task] (io_service* serv_ptr) {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            int cur_val = a;
            // If executes right now, a_mutex will recursively lock, and thus, proceed
            serv_ptr->dispatch(counting_task);
        };

    /*Services preparation*/

    io_service serv1;
    io_service serv2;

    std::vector<concurrency::jthread> threads1;
    std::vector<concurrency::jthread> threads2;

    // Tasks for service 1
    auto add_service1_tasks = 
        [&] () {
            for(int i = 0; i < num_tasks/2; ++i) {
                serv1.post(dispatch_task_local, &serv1); /*dispatch to self*/
                serv1.post(dispatch_task_foreign, &serv2); /*dispatch to other*/
            }
        };

    // Tasks for service 2
    auto add_service2_tasks = 
        [&] () {
            for(int i = 0; i < num_tasks/2; ++i) {
                serv2.post(dispatch_task_local, &serv2);
                serv2.post(dispatch_task_foreign, &serv1);
            }
        };

    // Workers for service 1
    auto add_service1_workers =
        [&] () {
            for(int i = 0; i < num_threads; ++i)
                threads1.emplace_back(worker_func, &serv1);
        };

    // Workers for service 2
    auto add_service2_workers =
        [&] () {
            for(int i = 0; i < num_threads; ++i)
                threads2.emplace_back(worker_func, &serv2);
        };

    auto finish_services =
        [&] () {
            using namespace std::chrono_literals;

            std::this_thread::sleep_for(10ms);

            serv1.stop();
            serv2.stop();
        };

    add_service2_workers();
    add_service1_tasks();
    add_service2_tasks();
    add_service1_workers();

    finish_services();

    std::cout << (a == num_iterations * tasks_complete) << std::endl;

    // // Shuffled order
    // add_service1_workers();
    // REQUIRE_THROWS(add_service1_tasks());
    // REQUIRE_THROWS(add_service2_tasks());

    // finish_services();

    // REQUIRE(a == num_iterations * tasks_complete);
   

    return 0;
}

// namespace io_service 
