#include "uring_async.hpp"

namespace io_service {
namespace detail {

thread_local int uring_res_ent::s_res_counter = 0;

} // namespace detail
} // namespace io_service
