
#include "scoped_uring.hpp"

namespace io_service {
namespace detail {

// Used for share io wq
uring_storage scoped_uring::S_storage;

} // namespace detail
} // namespace io_service
