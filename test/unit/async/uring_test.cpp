#include <catch2/catch_all.hpp>

#include <iostream>

#include "io_service.hpp"
#include "condition_variable"
#include "socket.hpp"
#include "unique_lock.hpp"
#include "mutex.hpp"
#include "jthread.hpp"

#include "acceptor.hpp"
#include "mutex.hpp"

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

TEST_CASE("acceptor creation") {
    const int port_num = 9999;
    const int num_cons = 10;

    io_service serv;

    ip::acceptor ac(serv);

    REQUIRE_NOTHROW(
        ac.bind(port_num));

    REQUIRE_NOTHROW(
        ac.listen(num_cons));
}

TEST_CASE("io_service & acceptor") {
    const int num_threads = 10;
    const int port_num = 9999;
    const int num_cons = 10;
    io_service serv;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    ip::acceptor ac(serv);

    concurrency::condition_variable cond_var;  
    concurrency::mutex mut;
    std::atomic<bool> done(false);

    ac.bind(port_num);
    ac.listen(num_cons);

    ac.async_accept(
        [&cond_var, &mut, &done](int err, ip::socket sock) {
            using namespace concurrency;
            // std::cout << "AAA" << std::endl;
            unique_lock<mutex> lk(mut);
            done = true;
            cond_var.notify_one();
        });

    // block until async_accept
    // std::cout << "IN" << std::endl;
    {
        using namespace concurrency;
        unique_lock<mutex> lk(mut);
        cond_var.wait(lk, [&done]() { return done.load(); });
    }
    // std::cout << "OUT" << std::endl;

    serv.stop();
}

} // namespace io_service
