#ifndef ASIO_LOGGER_HPP
#define ASIO_LOGGER_HPP

#include <fstream>
#include <ios>
#include <atomic>
#include <chrono>

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

    static const std::chrono::time_point<std::chrono::system_clock> s_begin;
    static thread_local detail::local_id s_local_id;

public:
    logger(std::string file_name)
        : m_file(c_path_base + ("/" +  file_name), std::ios_base::out)
    {}

public:
    void log(std::string msg) {
        using namespace concurrency;
        using nanos = std::chrono::nanoseconds;
        lock_guard<mutex> lk(m_mut);

        auto now = std::chrono::system_clock::now();
        m_file
            << std::chrono::duration_cast<nanos>(now - s_begin).count() << ": "
            << "thread " << s_local_id.get_id() << ": "
            << msg << std::endl;
    }

}; // class logger

class global_logger {
private:
    static logger m_logger;
    std::string m_prefix;

public:
    global_logger(std::string prefix)
        : m_prefix(std::move(prefix))
    {}

public:
    void log(std::string msg) 
    { m_logger.log(m_prefix + std::move(msg)); }

}; // class global_logger

} // namespace io_service

#endif
