#include "helgrind_annotations.hpp"
#include <catch2/catch_all.hpp>

#include <future>
#include <iostream> // std::cerr

#include <list>
#include <stdexcept>
#include <thread>
#include <chrono>

#include "io_service.hpp"

#include "jthread.hpp"
#include "shared_ptr.hpp"


namespace io_service {

static void worker_func(io_service* serv_ptr) {
    try
    {
        serv_ptr->run();
    }
    catch(const service_stopped_error& e)
    {
        // Should run() continue or abort (?)
        // std::cerr << e.what() << '\n';
    }
    catch(const std::runtime_error& e) {
        std::cerr << e.what() << '\n';
        REQUIRE(false); /*worker thread has unhandled exception*/
    }
}

static void worker_func_shr_ptr(memory::shared_ptr<io_service> serv_ptr) {
    try
    {
        serv_ptr->run();
    }
    catch(const service_stopped_error& e)
    {
        // Should run() continue or abort (?)
        // std::cerr << e.what() << '\n';
    }
    catch(const std::runtime_error& e) {
        std::cerr << e.what() << '\n';
        REQUIRE(false); /*worker thread has unhandled exception*/
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

        for(int i = 0; i < num_threads; ++i)
            threads.emplace_back(worker_func, &serv);

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
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    serv.stop();
    REQUIRE(a == tasks_count * num_iterations);
}

TEST_CASE("io_service: dispatch into own and foreign task pool", "[io_service][dispatch]") {
    const int num_iterations = 100;
    const int num_tasks = 50;
    const int num_threads = 20;

    using namespace std::chrono_literals;
    const std::chrono::milliseconds sleep_ms = 10ms;
    
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
            std::this_thread::sleep_for(sleep_ms);

            serv1.stop();
            serv2.stop();
            threads1.clear();
            threads2.clear();

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
    
    using namespace std::chrono_literals;
    const std::chrono::milliseconds sleep_ms = 10ms;

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
            std::this_thread::sleep_for(sleep_ms);

            serv.stop();
            thread_vec.clear();
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

template<typename T>
class sorter {
private:
	// Order matters, as pool should dstr first
	// in order to stop worker threads, so they could become joinable
	memory::shared_ptr<io_service> pool_ptr;
	std::vector<concurrency::jthread> threads;	

private:
	sorter(size_t num_of_threads)
		: pool_ptr( memory::make_shared<io_service>() ) 
	{
		for(size_t i = 0; i< num_of_threads; ++i)
			threads.push_back(
				concurrency::jthread(worker_func_shr_ptr, pool_ptr));
	}

	// This is needed to prevent
	// threads accessing deleted io_service.
	~sorter()
	{ pool_ptr->stop(); }

private:
	std::list<T> do_sort(std::list<T>& chunk_data) {
		if(chunk_data.empty())
			return chunk_data;

		std::list<T> result;
		result.splice(result.begin(), chunk_data, chunk_data.begin());
		T const& partition_val = *result.begin();

		typename std::list<T>::iterator divide_point =
			std::partition(chunk_data.begin(), chunk_data.end(),
				[&] (T const& val) { return val < partition_val; });

		std::list<T> new_lower_chunk;
		new_lower_chunk.splice(new_lower_chunk.end(),
			chunk_data, chunk_data.begin(), divide_point);

		std::future<std::list<T>> new_lower =
			/*used bind, since io_service::post cant refer to mem funcs*/
			pool_ptr->post_waitable(std::bind(&sorter::do_sort, this, std::move(new_lower_chunk)));
		
		std::list<T> new_higher(do_sort(chunk_data)); /*direct execution*/
		result.splice(result.end(), new_higher);

		while(new_lower.wait_for(std::chrono::seconds(0)) ==
			std::future_status::timeout
		) {
			pool_ptr->run_pending_task();
		}

		new_lower_chunk = new_lower.get();
		result.splice(result.begin(), new_lower_chunk);
		return result;
	}

private:
	template<typename D>
	friend
	std::list<D> parallel_quick_sort(std::list<D> input);

}; // struct sorter

template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
	const int num_threads = 5;

	if(input.empty())
		return input;

	sorter<T> sort(num_threads);
	std::list<T> result = sort.do_sort(input);

	return result;
}

TEST_CASE("sort data using io_service") {
	std::list<int> input_data = {312, 23, 512, 12, 42, 512, 0, -1};	

	std::list<int> potential_sorted =
		parallel_quick_sort(input_data);

	std::list<int> real_sorted = input_data;
	real_sorted.sort(); 

	REQUIRE_THAT(potential_sorted, Catch::Matchers::RangeEquals(real_sorted));
}

} // namespace io_service
