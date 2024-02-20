#include "logger.hpp"

namespace io_service {

std::atomic<int> detail::local_id::s_counter = 0;
thread_local detail::local_id logger::s_local_id;

} // namespace io_service
