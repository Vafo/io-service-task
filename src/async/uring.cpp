
#include "uring.hpp"

namespace io_service {

// Used for share io wq
std::list<int> uring::s_rings;
concurrency::mutex uring::s_rings_mut;

} // namespace io_service
