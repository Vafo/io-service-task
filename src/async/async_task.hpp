#ifndef ASIO_ASYNC_TASK
#define ASIO_ASYNC_TASK

#include "uring.hpp"
#include "function.hpp"

namespace io_service {

class async_task {
    func::function<void(uring&)> async_init;
    func::function<void(int)> async_compl;
}; // class async_task

} // namespace io_service

#endif
