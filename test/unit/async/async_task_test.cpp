#include <catch2/catch_all.hpp>

#include <iostream>

#include "io_service.hpp"
#include "generic_async.hpp"
#include "async_task.hpp"

#include "jthread.hpp"

namespace io_service {

int val = -1;
auto set_val = [] (async_result<int> res) {
    res.set_result(val);
};

auto get_val = [] (async_result<int> res) {
    REQUIRE(res.get_result() == val);
    REQUIRE_THROWS(res.set_result(2));
};

TEST_CASE("async_task creation") {
    async_result<int> res(get_val);

    async_task task(set_val, std::move(res)); 

    task();
}

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

// Check of compilation
TEST_CASE("generic async io_service") {
    io_service serv;
    generic_async_poster gen_post(serv);

    gen_post.post<int>(set_val, get_val);

    serv.stop();
}

// TODO: bad test.
// Cant test if none of async ops ever executed
TEST_CASE("generic async io_service executing") {
    const int num_threads = 10;
    const int num_async_tasks = 100;
    io_service serv;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    const int val = -123;

    std::atomic<int> set_num(0);
    std::atomic<int> get_num(0);
    std::atomic<bool> truth(true);

    auto set_val_count =
    [&set_num] (int val) {
        return
        [&set_num, val] (async_result<int> res) {
            res.set_result(val);
            ++set_num;
        };
    };

    auto get_val_count = 
    [&get_num, &truth] (int val) {
        return
        [&get_num, &truth, val] (async_result<int> res) {
            if(res.get_result() != val) {
                truth = false;
                return;
            }
            ++get_num;
        };
    };

    generic_async_poster poster(serv);
    for(int i = 0; i < num_async_tasks; ++i)
        poster.post<int>(set_val_count(i), get_val_count(i));

    serv.stop();

    REQUIRE(truth);
    REQUIRE(set_num == get_num);
}

} // namespace io_service
