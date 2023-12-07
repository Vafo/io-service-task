#include <catch2/catch_all.hpp>

#include <iostream>

#include "io_service.hpp"

#include "thread.hpp"

namespace io_service {

static void worker_func(io_service* serv_ptr) {
    serv_ptr->run();
}

TEST_CASE("io_service: creation and deletion", "[io_service]") {
    io_service serv;

    REQUIRE_NOTHROW(
        serv.post(
            [] (int a) -> void {
                a += 1;
            }, 
            1 /*a*/
        )
    );

    REQUIRE_NOTHROW( serv.stop() );
}

TEST_CASE("io_service: counting tasks", "[io_service]") {
    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 5;
    
    io_service serv;

    int a = 0;
    concurrency::mutex a_mutex;

    // post counting jobs
    for(int i = 0; i < num_tasks; ++i)
        serv.post(
            [&a, &a_mutex, num_iterations] () {
                using namespace concurrency;
                lock_guard<mutex> lock(a_mutex);
                for(int i = 0; i < num_iterations; ++i)
                    a += 1;
            });


    // add workers
    {
        std::vector<concurrency::jthread> threads;
        // how to avoid reservation (?)
        threads.reserve(num_threads);
        for(int i = 0; i < num_threads; ++i) {
            // how to deal with move constructor (?)
            // threads.push_back( std::move(concurrency::jthread(worker_func, &serv)) );
            threads.emplace_back(worker_func, &serv);
        }
            

        while(!serv.all_idle() || !serv.empty())
            ; /*wait for threads to finish tasks*/

        serv.stop();
    }

    REQUIRE(a == num_tasks * num_iterations);
}

TEST_CASE("io_service: dispatch", "[io_service]") {
    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 10;
    const int num_dispatch = 5;
    
    io_service serv;

    int a = 0;
    concurrency::mutex a_mutex;

    // post counting jobs
    auto task_func = 
        [&a, &a_mutex, num_iterations] () {
            using namespace concurrency;
            lock_guard<mutex> lock(a_mutex);
            for(int i = 0; i < num_iterations; ++i)
                a += 1;
        };

    for(int i = 0; i < num_tasks; ++i)
        serv.post(
            [task_func, num_dispatch, &serv, &a_mutex] () {
                using namespace concurrency;
                // lock_guard<mutex> lock(a_mutex);
                
                for(int disp_idx = 0; disp_idx < num_dispatch; ++disp_idx)
                    serv.dispatch(task_func);
            });


    // add workers
    std::vector<concurrency::jthread> threads;
    // how to avoid reservation (?)
    threads.reserve(num_threads);
    for(int i = 0; i < num_threads; ++i) {
        // how to deal with move constructor (?)
        // threads.push_back( std::move(concurrency::jthread(worker_func, &serv)) );
        threads.emplace_back(worker_func, &serv);
    }
        

    while(!serv.all_idle() || !serv.empty())
        ; /*wait for threads to finish tasks*/

    serv.stop();
    REQUIRE(a == num_tasks * num_dispatch * num_iterations);
}

TEST_CASE("io_service: dispatch into own and foreign task pool", "[io_service]") {
    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 10;
    
    int a = 0;
    concurrency::recursive_mutex a_mutex;

    /*Tasks definition*/

    // counting task
    auto counting_task = 
        [&a, &a_mutex, num_iterations] () {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            for(int i = 0; i < num_iterations; ++i)
                a += 1;
        };

    auto dispatch_task_local =
        [&a, &a_mutex, num_iterations, counting_task] (io_service* serv_ptr) {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            int cur_val = a;
            // If executes right now, a_mutex will recursively lock, and thus, proceed
            serv_ptr->dispatch(counting_task);

            REQUIRE(cur_val + num_iterations == a);
        };
    
    auto dispatch_task_foreign =
        [&a, &a_mutex, num_iterations, counting_task] (io_service* serv_ptr) {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            int cur_val = a;
            // If executes right now, a_mutex will recursively lock, and thus, proceed
            serv_ptr->dispatch(counting_task);

            REQUIRE(cur_val == a);
        };

    /*Services preparation*/

    io_service serv1;
    io_service serv2;

    std::vector<concurrency::jthread> threads1;
    threads1.reserve(num_threads);

    std::vector<concurrency::jthread> threads2;
    threads2.reserve(num_threads);

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
            while(
                !serv1.all_idle() || !serv1.empty() ||
                !serv2.all_idle() || !serv2.empty()
            )
                ;
                // std::cout << serv1.task_size() << " " << serv2.task_size() << std::endl; /*wait for threads to finish tasks*/

            serv1.stop();
            serv2.stop();
        };

    SECTION("Ordering 1") {
        add_service1_tasks();
        add_service2_tasks();
        add_service1_workers();
        add_service2_workers();

        finish_services();

        REQUIRE(a == num_tasks * num_iterations * 2/*num of services*/); // 
    }

    SECTION("Ordering 2") {
        add_service2_tasks();
        add_service1_tasks();
        add_service2_workers();
        add_service1_workers();

        finish_services();

        REQUIRE(a == num_tasks * num_iterations * 2/*num of services*/);
    }

    SECTION("Ordering 3") {
        add_service1_workers();
        add_service2_workers();
        add_service1_tasks();
        add_service2_tasks();

        finish_services();

        REQUIRE(a == num_tasks * num_iterations * 2/*num of services*/);
    }

    SECTION("Ordering 4") {
        add_service1_workers();
        add_service1_tasks();
        add_service2_workers();
        add_service2_tasks();

        finish_services();

        REQUIRE(a == num_tasks * num_iterations * 2/*num of services*/);
    }
}

} // namespace io_service