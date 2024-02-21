#include "logger.hpp"
#include <chrono>

namespace io_service {

std::atomic<int> detail::local_id::s_counter = 0;

const std::chrono::time_point<std::chrono::system_clock>
    logger::s_begin = std::chrono::system_clock::now();
thread_local detail::local_id logger::s_local_id;

logger global_logger::m_logger("global.log");

} // namespace io_service
