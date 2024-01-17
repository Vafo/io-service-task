#include <catch2/catch_all.hpp>
#include <future>
#include <vector>

#include "invocable.hpp"
#include "threadsafe_queue.hpp"

#include "jthread.hpp"


namespace io_service::new_impl {

TEST_CASE("queue creation") {
	const int test_val = 123;
	threadsafe_queue<int> iqueue;
	iqueue.push(test_val);

	int get_data;
	iqueue.wait_and_pop(get_data);

	REQUIRE(get_data ==	test_val);

	REQUIRE(iqueue.try_pop(get_data) == false);
}

TEST_CASE("queue accessed by 2 threads") {
	int const valid_sequence[] = {1, 2, 3, 4, 5, 5, 6, -1};
	size_t const valid_seq_size = 
		sizeof(valid_sequence) / sizeof(valid_sequence[0]);

	threadsafe_queue<int> queue;

	auto pusher_thread = [&] () {
		for(size_t i = 0; i < valid_seq_size; ++i)
			queue.push(valid_sequence[i]);
	};

	auto receiver_thread = [&] () {
		int tmp_int;
		for(size_t i = 0; i < valid_seq_size; ++i) {
			queue.wait_and_pop(tmp_int);
			REQUIRE(valid_sequence[i] == tmp_int);
		}
	};

	using namespace concurrency;
	jthread tr1(pusher_thread);
	jthread tr2(receiver_thread);
}

TEST_CASE("queue accessed by multiple threads") {
	int const arr_size = 4096;
	int const pushers_num = 10;
	int const executors_num = 100;

	// TODO: Find out why it does not work with std::vector
	// Potentially because of memory modification order
	// All of the data is in the same cache line,
	// 	access sequenced by atomic counter
	// But vector has its data in heap, so compiler can't sequence access
	// It should be done explicitly, somehow
	// typedef std::vector<bool>::iterator vec_iter;
	// std::vector<bool> bvec(arr_size, false);

	typedef bool* vec_iter;
	bool bvec[arr_size];
	for(size_t i = 0; i < arr_size; ++i)
		bvec[i] = false;

	std::atomic<int> tasks_done(0);

	threadsafe_queue<invocable> queue; 

	auto set_true_task =
		[] (vec_iter iter) {
			*iter = true;
		};

	auto pusher_thread =
		[&queue, &set_true_task] (vec_iter begin, vec_iter end) {
			while(begin != end) {
				std::packaged_task<void(vec_iter)> task(set_true_task);
				queue.push(invocable(std::move(task), begin));
				++begin;
			}
		};

	auto executor_thread =
		[&queue, arr_size, &tasks_done] () {
			while(tasks_done < arr_size) {
				invocable task;
				// task queue might become empty by this time
				if(!queue.try_pop(task))
					continue;
				// execute task
				task();
				++tasks_done;
			}
		};

	{
		using namespace concurrency;
		std::vector<jthread> pushers;
		std::vector<jthread> executors;
		
		int const total_blocks = arr_size /*bvec.size()*/;
		int const block_size = total_blocks / pushers_num; 
		vec_iter b_st = bvec /*bvec.begin()*/;
		vec_iter b_end = b_st;
		for(int i = 0; i < (pushers_num-1); ++i) {
			std::advance(b_end, block_size);
			pushers.push_back(jthread(pusher_thread, b_st, b_end));
			b_st = b_end;
		}
		pushers.push_back(jthread(pusher_thread, b_end, bvec + arr_size/*bvec.end()*/));

		for(int i = 0; i < executors_num; ++i)
			executors.push_back(jthread(executor_thread));
	}

	// safe to check vector
	bool all_true = true;
	for(vec_iter iter = bvec /*bvec.begin()*/; iter != bvec + arr_size/*bvec.end()*/; ++iter)
		all_true = all_true && *iter;

	REQUIRE(all_true);
};

} // namespace io_service::new_impl
