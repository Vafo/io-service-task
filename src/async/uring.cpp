
#include "uring.hpp"

namespace io_service {

std::list<int> uring::s_rings;
concurrency::mutex uring::s_rings_mut;

} // namespace io_service
