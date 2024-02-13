#include <iostream>
#include <vector>
#include <functional>

#include "io_service.hpp"
#include "jthread.hpp"

#include "acceptor.hpp"
#include "socket.hpp"

void worker_func(io_service::io_service* service_ptr) {
    service_ptr->run();
}

class connections_manager {
private:
    io_service::io_service& m_serv;
    io_service::ip::acceptor m_ac;
    std::vector<io_service::ip::socket> m_socks;

public:
    connections_manager(io_service::io_service& serv)
        : m_serv(serv)
        , m_ac(serv)
    {}

public:
    void start_connections(in_port_t port) {
        m_ac.bind(port);
        m_ac.listen(10);
        using namespace std::placeholders;
        m_ac.async_accept(
            std::bind(&connections_manager::handle_connection, this, _1, _2));
    }


private:
    void handle_connection(int err, io_service::ip::socket sock) {
        std::cout << "new connection" << std::endl;
        m_socks.push_back(std::move(sock));
        using namespace std::placeholders;
        m_ac.async_accept(
            std::bind(&connections_manager::handle_connection, this, _1, _2));
    }

}; // class connections_manager

int main(int argc, char* argv[]) {
    const int num_threads = 1;
    const int port_num = 9999;
    const int num_cons = 10;

    io_service::io_service serv;
    std::cout << "what" << std::endl;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    concurrency::condition_variable cond_var;  
    concurrency::mutex mut;
    std::atomic<bool> done(false);

    connections_manager mngr(serv);
    mngr.start_connections(9999);
    std::cout << "dakldw" << std::endl;

    // block until async_accept
    // std::cout << "IN" << std::endl;
    {
        using namespace concurrency;
        unique_lock<mutex> lk(mut);
        cond_var.wait(lk, [&done]() { return done.load(); });
    }
    // std::cout << "OUT" << std::endl;

    serv.stop();
    return 0;
}

// namespace io_service 
