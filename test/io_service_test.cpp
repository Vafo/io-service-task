#include <catch2/catch_all.hpp>

#include <iostream>
#include <condition_variable>

#include "io_service.hpp"

#include "thread.hpp"

namespace io_service {

static void worker_func(io_service* serv_ptr) {
    try
    {
        serv_ptr->run();
    }
    catch(const std::exception& e)
    {
        // TODO: Wait for specific exception: post to stopped io_service
        // Should run() continue or abort (?)
        std::cerr << e.what() << '\n';
    }
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
    const int num_threads = 5;
    const int num_iterations = 100;
    const int num_tasks = 50;

    io_service serv;

    int a = 0;
    int tasks_count = 0;
    concurrency::mutex a_mutex;

    // post counting jobs
    for(int i = 0; i < num_tasks; ++i)
        serv.post(
            [&a, &a_mutex, num_iterations, &tasks_count] () {
                using namespace concurrency;
                lock_guard<mutex> lock(a_mutex);
                for(int i = 0; i < num_iterations; ++i)
                    a += 1;

                tasks_count++;
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

        serv.stop();
    }

    REQUIRE(a == tasks_count * num_iterations);
}

TEST_CASE("io_service: dispatch", "[io_service]") {
    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 10;
    const int num_dispatch = 5;
    
    io_service serv;

    int a = 0;
    int tasks_count = 0;
    concurrency::mutex a_mutex;

    // post counting jobs
    auto task_func = 
        [&a, &a_mutex, num_iterations, &tasks_count] () {
            using namespace concurrency;
            lock_guard<mutex> lock(a_mutex);
            for(int i = 0; i < num_iterations; ++i)
                a += 1;

            tasks_count++;
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
    // // how to avoid reservation (?)
    // threads.reserve(num_threads);
    for(int i = 0; i < num_threads; ++i) {
        // how to deal with move constructor (?)
        // threads.push_back( std::move(concurrency::jthread(worker_func, &serv)) );
        threads.emplace_back(worker_func, &serv);
    }

    serv.stop();
    REQUIRE(a == tasks_count * num_iterations);
}

TEST_CASE("io_service: dispatch into own and foreign task pool", "[io_service][dispatch]") {
    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 20;
    
    int a = 0;
    int tasks_complete = 0;
    // Validity of dispatch
    bool is_dispatch_local_valid = true;
    bool is_dispatch_foreign_valid = true;
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
        [&a, &is_dispatch_local_valid, &a_mutex, num_iterations, counting_task]
        (io_service* serv_ptr) {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            int cur_val = a;
            // If executes right now, a_mutex will recursively lock, and thus, proceed
            serv_ptr->dispatch(counting_task);

            is_dispatch_local_valid = is_dispatch_local_valid && (cur_val + num_iterations == a);
        };
    
    auto dispatch_task_foreign =
        [&a, &is_dispatch_foreign_valid, &a_mutex, num_iterations, counting_task] (io_service* serv_ptr) {
            using namespace concurrency;
            lock_guard<recursive_mutex> lock(a_mutex);
            int cur_val = a;
            // If executes right now, a_mutex will recursively lock, and thus, proceed
            serv_ptr->dispatch(counting_task);

            is_dispatch_foreign_valid = is_dispatch_foreign_valid && (cur_val == a);
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
            // Not really reliable
            // It wont busy loop good enough to let all tasks to be completed
            // while(
            //     !serv1.empty() || !serv1.all_idle() ||
            //     !serv2.empty() || !serv2.all_idle() 
            // )
            //     ;
            // TODO: Add sleep

            serv1.stop();
            serv2.stop();

            REQUIRE(is_dispatch_local_valid);
            REQUIRE(is_dispatch_foreign_valid);
        };


    /*Tasks & Workers execution*/

    SECTION("Ordering 1") {
        add_service1_tasks();
        add_service2_tasks();
        add_service1_workers();
        add_service2_workers();

        finish_services();

        REQUIRE(a == num_iterations * tasks_complete);
    }

    SECTION("Ordering 2") {
        add_service2_tasks();
        add_service1_tasks();
        add_service2_workers();
        add_service1_workers();

        finish_services();

        REQUIRE(a == num_iterations * tasks_complete);
    }

    SECTION("Ordering 3") {
        add_service1_workers();
        add_service2_workers();
        add_service1_tasks();
        add_service2_tasks();

        finish_services();

        REQUIRE(a == num_iterations * tasks_complete);
    }

    SECTION("Ordering 4") {
        add_service1_workers();
        add_service1_tasks();
        add_service2_workers();
        add_service2_tasks();

        finish_services();

        REQUIRE(a == num_iterations * tasks_complete);
    }
}

TEST_CASE("io_service: restart empty service") {
    io_service serv;

    REQUIRE_NOTHROW(serv.restart());
}

TEST_CASE("io_service: service reusage", "[io_service][restart]") {
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
    
    io_service serv;
    std::vector<concurrency::jthread> thread_vec;

    auto add_tasks = 
        [&] () {
            for(int i = 0; i < num_tasks; ++i)
                serv.post(counting_task);
        };

    auto add_workers =
        [&] () {
            for(int i = 0; i < num_threads; ++i)
                thread_vec.emplace_back(worker_func, &serv);
        };


    auto finish_service =
        [&] () {
            // while(
            //     !serv.empty() || !serv.all_idle()
            // )
            //     ;
            // TODO: Add sleep

            serv.stop();
        };

    add_tasks();
    add_workers();
    finish_service();
    
    REQUIRE(a == num_iterations * tasks_complete);

    SECTION("Reuse non restarted service") {
            int capture_tasks_complete = tasks_complete;
            add_workers();
            REQUIRE_THROWS(add_tasks());

            finish_service();

            REQUIRE(a == num_iterations * capture_tasks_complete);
    }

    SECTION("Reuse restarted service") {
            serv.restart();
            REQUIRE_NOTHROW(add_tasks());
            add_workers();

            finish_service();

            REQUIRE(a == num_iterations * tasks_complete);
    }
}

} // namespace io_service