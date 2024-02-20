#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <csignal>

#include "buffer.hpp"
#include "condition_variable.hpp"
#include "jthread.hpp"
#include "mutex.hpp"

#include "io_service.hpp"
#include "acceptor.hpp"
#include "socket.hpp"
#include "uring.hpp"
#include "logger.hpp"

void worker_func(io_service::io_service* service_ptr) {
    service_ptr->run();
}

class echo_connection 
    : public std::enable_shared_from_this<echo_connection>
{
private:
    io_service::ip::socket m_sock;
    char m_raw_data[128];
    size_t m_size;

    int m_id;
    static std::atomic<int> s_id;

    io_service::logger& m_log;
    std::string sock_msg;

public:
    explicit
    echo_connection(io_service::ip::socket&& sock, io_service::logger& log)
        : m_sock(std::forward<io_service::ip::socket>(sock))
        , m_id(s_id++)
        , m_log(log)
        , sock_msg("sock " + std::to_string(m_id) + ": ")
    {}

public:
    void start_echo() {
        M_init_read();
    }

private:
    void M_init_read() {
        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " start");

        using namespace std::placeholders;
        auto self(shared_from_this());
        m_sock.async_read_some(io_service::buffer(m_raw_data, sizeof(m_raw_data)),
            std::bind(&echo_connection::M_handle_read, self, _1, _2));

        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " end");
    }

    void M_handle_read(io_service::uring_error err, int size) {
        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " start");

        if(err) {
            std::cerr << "could not read " << strerror(-err.value()) << std::endl; 
            return;
        }

        if(size == 0) {
            // close connection
            std::cerr << "read: finishing con" << std::endl;
            return;
        }

        m_size = size;
        M_init_write();

        m_log.log(sock_msg +__PRETTY_FUNCTION__ + " end");
    }

    void M_init_write() {
        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " start");

        using namespace std::placeholders;
        auto self(shared_from_this());
        m_sock.async_write_some(io_service::buffer(m_raw_data, m_size),
            std::bind(&echo_connection::M_handle_write, self, _1, _2));

        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " end");
    }

    void M_handle_write(io_service::uring_error err, int size) {
        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " start");

        if(err) {
            if (err.value() == EPERM) {
                std::cerr << "write: finishing con" << std::endl;
                return;
            }

            std::cerr << "could not write " << strerror(-err.value()) << std::endl; 
            return;
        }

        if(size != m_size) {
            std::cerr << "short write " << size << " " << m_size << std::endl; 
            return;
        }

        m_size = 0;
        M_init_read();

        m_log.log(sock_msg + __PRETTY_FUNCTION__ + " end");
    }

}; // class connection

std::atomic<int> echo_connection::s_id = 0;

class connections_manager {
private:
    io_service::io_service& m_serv;
    io_service::ip::acceptor m_ac;
    std::list<echo_connection> m_cons;

    concurrency::mutex& m_cond_var_mut;
    concurrency::condition_variable& m_cond_var;

    std::atomic<int> m_count;

    io_service::logger& m_log;

public:
    connections_manager(
        io_service::io_service& serv,
        concurrency::mutex& mut,
        concurrency::condition_variable& cond_var,
        io_service::logger& log
    )
        : m_serv(serv)
        , m_ac(serv)
        , m_cond_var_mut(mut)
        , m_cond_var(cond_var)
        , m_count(0)
        , m_log(log)
    {}

public:
    void start_connections(in_port_t port, int num_cons) {
        m_log.log(__PRETTY_FUNCTION__ + std::string(" start"));

        m_ac.bind(port);
        m_ac.listen(num_cons);

        m_log.log(__PRETTY_FUNCTION__ + std::string(" end"));
    }

    void accept_connections() {
        m_log.log(__PRETTY_FUNCTION__ + std::string(" start"));

        using namespace std::placeholders;
        m_ac.async_accept(
            std::bind(&connections_manager::handle_connection, this, _1, _2));

        m_log.log(__PRETTY_FUNCTION__ + std::string(" end"));
    }

private:
    void handle_connection(int err, io_service::ip::socket sock) {
        m_log.log(__PRETTY_FUNCTION__ + std::string(" start"));

        if(err) {
            std::cerr << "could not connect err " << strerror(err) << std::endl;
        }

        std::cout << ++m_count << std::endl;

        accept_connections();
        std::make_shared<echo_connection>(std::move(sock), m_log)->start_echo();

        m_log.log(__PRETTY_FUNCTION__ + std::string(" end"));
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
    const int num_threads = 5;
    const int port_num = 9999;
    const int num_cons = 3;

    std::signal(SIGINT, sigint_handler);

    io_service::io_service serv;
    std::vector<concurrency::jthread> threads;
    
    for(int i = 0; i < num_threads; ++i)
        threads.emplace_back(worker_func, &serv);

    io_service::logger log("aboba.log");
    log.log("Aboba");

    connections_manager mngr(serv, mut, cond_var, log);
    mngr.start_connections(port_num, num_cons);
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
