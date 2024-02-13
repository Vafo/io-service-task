#include <catch2/catch_all.hpp>

#include "monitor.hpp"

#include "jthread.hpp"
#include "shared_ptr.hpp"

namespace io_service {

TEST_CASE("monitor creation") {
    const int save_val = 123;
    monitor<int> m_int;
    m_int([](int& val) {
        val = save_val;
    });

    int get_val = m_int([](int& val) {
            return val;
        });

    REQUIRE(get_val == save_val);
}

TEST_CASE("monitor multiple access") {
    using namespace concurrency;
    const int init_val = 123;
    const int num_threads = 10;
    const int increment_batch = 100;

    typedef memory::shared_ptr<monitor<int>> counter_ptr_type;
    counter_ptr_type counter_ptr = 
        memory::make_shared<monitor<int>>(init_val);
    {
        std::vector<jthread> trs;

        auto increment_job = [increment_batch] (int& val) {
            for(int i = 0; i < increment_batch; ++i)
                ++val;
        };

        for(int i = 0; i < num_threads; ++i)
            trs.push_back(
                jthread([increment_job] (counter_ptr_type ptr) {
                    ptr->operator()(increment_job);
                }, counter_ptr));

        // wait for threads to join
    }

   counter_ptr->operator()(
        [init_val, num_threads, increment_batch] (int& val) {
            REQUIRE(val == init_val + (num_threads * increment_batch));
        });
}

} // namespace io_service
