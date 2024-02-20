#ifndef ASIO_LOGGER_HPP
#define ASIO_LOGGER_HPP

#include <fstream>
#include <ios>
#include <thread>

#include "lock_guard.hpp"
#include "mutex.hpp"

#include <iostream>

namespace io_service {

namespace detail {

class local_id {
private:
    static std::atomic<int> s_counter;
    int m_id;

public:
    local_id()
        : m_id(s_counter++)
    {}

public:
    int get_id()
    { return m_id; }

}; // class local_id

}; 

class logger {
private:
    const char* c_path_base = "logs";
private:
    std::fstream m_file;
    concurrency::mutex m_mut;

    static thread_local detail::local_id s_local_id;

public:
    logger(std::string file_name)
        : m_file(c_path_base + ("/" +  file_name), std::ios_base::out)
    {}

public:
    void log(std::string msg) {
        using namespace concurrency;
        lock_guard<mutex> lk(m_mut);

        m_file
            << "thread " << s_local_id.get_id() << ": "
            << msg << std::endl;
    }

}; // class logger

} // namespace io_service

#endif
