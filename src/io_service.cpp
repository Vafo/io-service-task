#include "io_service.hpp"

namespace io_service {

void io_service::run() {
    using namespace concurrency;

    while(true) {
        invocable cur_task;

        /*wait for task*/
        {
            unique_lock<mutex> lock(m_queue_mutex);
            m_queue_cv.wait(
                lock,
                [&] () { return m_queue.size() > 0; }
            );
            cur_task = m_queue.front();
            m_queue.pop();
        }

        /*execute task*/
        cur_task();

        
    }
}

} // namespace io_service