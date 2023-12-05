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
            

        while(!serv.empty())
            ; /*wait for threads to finish tasks*/

        serv.stop();
    }

    REQUIRE(a == num_tasks * num_iterations);
}

TEST_CASE("io_service: dispatch", "[io_service]") {
    const int num_iterations = 100;
    const int num_tasks = 2;
    const int num_threads = 5;
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
    {
        std::vector<concurrency::jthread> threads;
        // how to avoid reservation (?)
        threads.reserve(num_threads);
        for(int i = 0; i < num_threads; ++i) {
            // how to deal with move constructor (?)
            // threads.push_back( std::move(concurrency::jthread(worker_func, &serv)) );
            threads.emplace_back(worker_func, &serv);
        }
            

        while(!serv.empty())
            ; /*wait for threads to finish tasks*/

        serv.stop();
    }

    REQUIRE(a == num_tasks * num_dispatch * num_iterations);
}

} // namespace io_service