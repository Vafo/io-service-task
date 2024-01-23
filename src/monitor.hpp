#ifndef ASIO_MONITOR_HPP
#define ASIO_MONITOR_HPP

#include "lock_guard.hpp"
#include "mutex.hpp"
#include <type_traits>

namespace io_service {

// Provides mutual exclusive access to data type T
template<typename T>
class monitor {
private:
    T m_data;
    concurrency::mutex mut;

public:
    monitor()
        : m_data()
    {}

    monitor(T data)
        : m_data(std::move(data))
    {}

    template<typename Callable>
    std::result_of_t<Callable(T&)> operator()(Callable func) {
        using namespace concurrency;
        lock_guard<mutex> lk(mut);
        return func(m_data);
    }

}; // class monitor

} // namespace io_service

#endif
