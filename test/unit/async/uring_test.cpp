#include <catch2/catch_all.hpp>

#include <iostream>
#include <string>
#include <utility>

#include "function.hpp"
#include "io_service.hpp"
#include "resolver.hpp"
#include "socket.hpp"
#include "unique_lock.hpp"
#include "mutex.hpp"
#include "jthread.hpp"

#include "acceptor.hpp"
#include "socket.hpp"
#include "async_connect.hpp"

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

class socket_connector {
private:
    ip::resolver m_res;
    ip::socket m_sock;
    func::function<void()> m_handler;

    ip::resolver::results_type m_eps;

public:
    template<typename Callback>
    socket_connector(io_service& serv, Callback&& cb)
        : m_res(serv)
        , m_sock(serv)
        , m_handler(std::forward<Callback>(cb))
    {}

public:
    void connect(std::string hostname, std::string port) {
        using namespace std::placeholders;
        m_res.async_resolve(hostname, port,
            std::bind(&socket_connector::handle_resolve, this, _1));
    }

private:
    void handle_connect(int res) {
        REQUIRE(res == 0);
        m_handler();
    }

    void handle_resolve(async_result<ip::resolver::results_type> res) {
        using namespace std::placeholders;
        m_eps = std::move(res.get_result());
        m_sock.setup();
        ip::async_connect(m_sock, m_eps,
            std::bind(&socket_connector::handle_connect, this, _1));
    }

}; // socket_connector

TEST_CASE("io_service & acceptor") {
    const int num_threads = 10;
    const int port_num = 9998;
    const int num_cons = 10;
    io_service serv;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    ip::acceptor ac(serv);

    concurrency::condition_variable cond_var;  
    concurrency::mutex mut;
    bool done = false;
    
    std::atomic<bool> accepting(false);
    std::atomic<bool> connected(false);

    ac.bind(port_num);
    ac.listen(num_cons);

    ac.async_accept(
        [&accepting](int err, ip::socket sock) {
            
            if(!err)
               accepting = true; 
            
        });

    socket_connector con(serv,
        [&cond_var, &mut, &done, &connected] () {
            using namespace concurrency;
            unique_lock<mutex> lk(mut);
            done = true;
            connected = true;
            cond_var.notify_one();
        });

    con.connect("localhost", std::to_string(port_num));

    // block until async_accept
    {
        using namespace concurrency;
        unique_lock<mutex> lk(mut);
        cond_var.wait(lk, [&done]() { return done; });
    }

    REQUIRE(accepting);
    REQUIRE(connected);

    serv.stop();
}

} // namespace io_service
