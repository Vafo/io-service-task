#ifndef CHECKED_DELETE_H
#define CHECKED_DELETE_H

namespace io_service::util {

template<typename T>
void check_if_deletable(T *ptr) {
    typedef char type_must_be_complete[ sizeof(T) ? 1 : -1 ];
    (void) sizeof(type_must_be_complete);
}

} // namespace io_service::util

#endif