#include <catch2/catch_all.hpp>
#include <vector>

#include "strand.hpp"
#include "io_service.hpp"

#include "jthread.hpp"
#include "shared_ptr.hpp"

namespace io_service {

TEST_CASE("strand creation") {
    const int num_threads = 5;
    const int num_tasks = 10;
    const int incr_num = 1000;

    memory::shared_ptr<io_service> srvc_ptr = 
        memory::make_shared<io_service>();

    int test_var = 0;
    strand<io_service> test_strand(*srvc_ptr);

    auto worker_thread =
        [] (memory::shared_ptr<io_service> srvc_ptr)
        { srvc_ptr->run(); };

    std::vector<concurrency::jthread> trs;
    for(int i = 0; i < num_threads; ++i)
        trs.push_back(concurrency::jthread(worker_thread, srvc_ptr));


    auto incr_var_handle = 
        [incr_num, &test_var] () {
            for(int i = 0; i < incr_num; ++i)
                ++test_var;
        };

    for(int i = 0; i < num_tasks; ++i)
        test_strand.post(incr_var_handle);

    srvc_ptr->stop();

    REQUIRE(test_var == (num_tasks * incr_num));
}


} // namespace io_service
