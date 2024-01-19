#ifndef ASIO_FALSE_FUNC_HPP
#define ASIO_FALSE_FUNC_HPP

// Functor that always returns false
struct false_func {
	bool operator()()
	{ return false; }
};


#endif
