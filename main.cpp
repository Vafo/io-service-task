#include <iostream>
#include <vector>
#include <functional>
#include <csignal>

#include "condition_variable.hpp"
#include "io_service.hpp"
#include "jthread.hpp"

#include "acceptor.hpp"
#include "mutex.hpp"
#include "socket.hpp"

void worker_func(io_service::io_service* service_ptr) {
    service_ptr->run();
}

class connections_manager {
private:
    io_service::io_service& m_serv;
    io_service::ip::acceptor m_ac;
    std::vector<io_service::ip::socket> m_socks;

    concurrency::mutex& m_cond_var_mut;
    concurrency::condition_variable& m_cond_var;

public:
    connections_manager(
        io_service::io_service& serv,
        concurrency::mutex& mut,
        concurrency::condition_variable& cond_var
    )
        : m_serv(serv)
        , m_ac(serv)
        , m_cond_var_mut(mut)
        , m_cond_var(cond_var)
    {}

public:
    void start_connections(in_port_t port) {
        m_ac.bind(port);
        m_ac.listen(10);
    }

    void accept_connections() {
        using namespace std::placeholders;
        m_ac.async_accept(
            std::bind(&connections_manager::handle_connection, this, _1, _2));
    }

private:
    void handle_connection(int err, io_service::ip::socket sock) {
        std::cout << "new connection" << std::endl;
        m_socks.push_back(std::move(sock));

        accept_connections();
    }

}; // class connections_manager



std::atomic<bool> done(false);
concurrency::condition_variable cond_var;  
concurrency::mutex mut;

extern "C"
void sigint_handler(int sig) {
    using namespace concurrency;
    unique_lock<mutex> lk(mut);
    done = true;
    cond_var.notify_one();
}

int main(int argc, char* argv[]) {
    const int num_threads = 1;
    const int port_num = 9999;
    const int num_cons = 10;

    std::signal(SIGINT, sigint_handler);

    io_service::io_service serv;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    connections_manager mngr(serv, mut, cond_var);
    mngr.start_connections(9999);
    mngr.accept_connections();

    // block until async_accept
    {
        using namespace concurrency;
        unique_lock<mutex> lk(mut);
        cond_var.wait(lk, [] () { return done.load()/*bool*/; });
    }

    std::cout << "terminating" << std::endl;
    serv.stop();
    return 0;
}

// namespace io_service 
