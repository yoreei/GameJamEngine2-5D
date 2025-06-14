#pragma once

#include <chrono>
#include <iterator>
#include <random>
#include <unordered_map>
#include <string>
#include <tuple>
#include <span>
#include <iostream>
#include <cassert>

#ifndef NDEBUG
inline constexpr bool DEBUG = true;
#else
inline constexpr bool DEBUG = false;
#endif


#if defined(DISABLE_INLINING)
#if defined(_MSC_VER)
#define INLINING __declspec(noinline)
#else
#define INLINING __attribute__((noinline))
#endif
#else
#define INLINING 
#endif

#define fastIO ios::sync_with_stdio(false); cin.tie(nullptr);


#define ASSERT_EXPR(condition, ...)                              \
      [&](decltype((condition))&& value) -> decltype((condition)) {  \
       assert(value);                                                \
       return std::forward<decltype(value)>(value);                  \
      }(condition)


/************************************
**** CONVERSIONS
************************************/

#define UNUSED(param)
#define toU8(x) static_cast<uint8_t>(x)
#define toU32(x) static_cast<uint32_t>(x)
#define toF(x) static_cast<float>(x)

/************************************
**** CONSTANTS
************************************/

constexpr float fPi = 3.141592653589793f;

/************************************
**** TIME
************************************/

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

inline TimePoint getTimePoint() {
	return std::chrono::high_resolution_clock::now();
}

using Micro = std::chrono::duration<double, std::micro>;
using Milli = std::chrono::duration<double, std::milli>;
using Seconds = std::chrono::duration<double, std::ratio<1>>;

template <typename Unit>
inline Unit getEpochTime() {
	return getTimePoint().time_since_epoch();
}

// vBench
struct IdentityHasher {
	size_t operator()(size_t key) const noexcept {
		return key;
	}
};
using BenchId = size_t;
static std::unordered_map<BenchId, std::tuple<std::string, double>, IdentityHasher> cppBenchMap;
static std::unordered_map<BenchId, double, IdentityHasher> cppBenchActive;

/* not thread-safe! */
inline BenchId cppBench(const std::string& name) {
	BenchId hashValue = std::hash<std::string>{}(name);
	if (cppBenchMap.find(hashValue) == cppBenchMap.cend()) {
		std::get<std::string>(cppBenchMap[hashValue]) = name;
		std::get<double>(cppBenchMap[hashValue]) = 0.f;
	}
	cppBenchActive[hashValue] = getEpochTime<Milli>().count();
	return hashValue;
}

inline void cppBenchEnd(BenchId id) {
	if (cppBenchActive.find(id) == cppBenchActive.cend()) {
		throw std::runtime_error("wrong bench id");
	}
	std::get<double>(cppBenchMap[id]) += getEpochTime<Milli>().count() - cppBenchActive[id];
	cppBenchActive.erase(id);
}

inline void cppBenchPrint() {
	if (cppBenchActive.size() != 0) {
		std::cout << "cppBenchActive is not empty!!!\n";
	}
	for (const auto& e : cppBenchMap) {
		// e.second is a <std::string, double> tuple
		double time = std::get<double>(e.second); // Milli 
		std::string unit = "ms";

		if (time >= 1000.f) {
			time /= 1000.f;
			unit = "s";
		}

		if (time >= 60) {
			time /= 60;
			unit = "m";
		}

		std::cout << std::get<std::string>(e.second) << ": " << time << unit << "\n";
	}
}
// ^Bench

/*****************************************************
**** BITS
******************************************************/

inline uint32_t swapEndian(uint32_t val) {
	return ((val >> 24) & 0xff) |
		((val << 8) & 0xff0000) |
		((val >> 8) & 0xff00) |
		((val << 24) & 0xff000000);
}

// Convert a pixel (float) to a character for visualization.
inline char ASCIIArtFromFloat(float f) {
	if (f > 0.7f) {
		return '#';
	}
	else if (f > 0.4f) {
		return '!';
	}
	else if (f > 0.1f) {
		return '.';
	}
	else if (f >= 0) {
		return ' ';
	}
	else {
		throw std::runtime_error("wrong f");
	}
}

inline void enableFpExcept() {
#if defined(_MSC_VER) && !defined(__clang__)
	// Clear the exception masks for division by zero, invalid operation, and overflow.
	// This means these exceptions will be raised.

	// _EM_INEXACT
	// _EM_UNERFLOW
	// _EM_DENORMAL
	unsigned int current;
	_controlfp_s(&current, 0, 0);
	_controlfp_s(&current, current & ~(_EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW), _MCW_EM);
#endif
}

/********************************************************
**** RANDOM
*******************************/

template <typename Iterator>
inline void randSeq(Iterator begin, Iterator end, std::iter_value_t<Iterator> rMin = 0, std::iter_value_t<Iterator> rMax = 1) {
	using T = std::iter_value_t<Iterator>;
	static std::random_device rd;
	static std::mt19937 gen(rd());

	// Static Distribution Selection
	using Distribution = std::conditional_t<
		std::is_integral_v<T>, std::uniform_int_distribution<T>,
		std::conditional_t<
		std::is_floating_point_v<T>, std::uniform_real_distribution<T>,
		void>>;


	static_assert(!std::is_same_v<Distribution, void>,
		"T must be an integral or floating-point type");

	Distribution dist(rMin, rMax);
	while (begin != end) {
		*begin = dist(gen);
		++begin;
	}
}
/**************************************
**** LOGGING
**************************************/

template<typename T>
inline void cppLog(const std::span<T>& arr) {
	std::cout << "[";
	for (size_t i = 0; i < arr.size(); ++i) {
		std::cout << arr[i] << ", ";
	}
	std::cout << "]\n";

}

// A CRTP base class that instruments the special member functions.
template <typename Derived>
struct Traceable {
	inline Traceable() { TraceableLog(std::string(typeid(Derived).name()) + ": default constructed"); }
	inline Traceable(const Traceable&) { TraceableLog(std::string(typeid(Derived).name()) + ": copy constructed"); }
	inline Traceable(Traceable&&) { TraceableLog(std::string(typeid(Derived).name()) + ": move constructed"); }
	inline Traceable& operator=(const Traceable&) {
		TraceableLog(std::string(typeid(Derived).name()) + ": copy assigned");
		return *this;
	}
	inline Traceable& operator=(Traceable&&) {
		TraceableLog(std::string(typeid(Derived).name()) + ": move assigned");
		return *this;
	}
	inline ~Traceable() { TraceableLog(std::string(typeid(Derived).name()) + ": destructed"); }
	inline static void TraceableLog(const std::string& msg) {
		//std::cout << msg << std::endl;
	}
};

/*************************************
**** BITMASK FLAGS (ENUMS)
*************************************/
/// Usage (how to opt-in for your enum):
/// template<> struct bitmask_enabled<MyFlags> : std::true_type { };
/// do not forget to default-init enum values!

#include <type_traits>
#include <concepts>

// 1) A trait you specialize to opt your enum into bitmask ops:
template<typename E>
struct bitmask_enabled : std::false_type {};

// 2) A concept that checks its an enum and that youve opted it in:
template<typename E>
concept BitmaskEnum = std::is_enum_v<E> && bitmask_enabled<E>::value;

// 3) Helper to get the underlying integer:
template<BitmaskEnum E>
constexpr auto to_underlying(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}

// 4) Define all the usual bitwise operators:
template<BitmaskEnum E>
constexpr E operator|(E lhs, E rhs) noexcept {
    return static_cast<E>(to_underlying(lhs) | to_underlying(rhs));
}
template<BitmaskEnum E>
constexpr E operator&(E lhs, E rhs) noexcept {
    return static_cast<E>(to_underlying(lhs) & to_underlying(rhs));
}
template<BitmaskEnum E>
constexpr E operator^(E lhs, E rhs) noexcept {
    return static_cast<E>(to_underlying(lhs) ^ to_underlying(rhs));
}
template<BitmaskEnum E>
constexpr E operator~(E v) noexcept {
    return static_cast<E>(~to_underlying(v));
}

template<BitmaskEnum E>
constexpr E& operator|=(E& lhs, E rhs) noexcept {
    lhs = lhs | rhs; 
    return lhs;
}
template<BitmaskEnum E>
constexpr E& operator&=(E& lhs, E rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}
template<BitmaskEnum E>
constexpr E& operator^=(E& lhs, E rhs) noexcept {
    lhs = lhs ^ rhs;
    return lhs;
}

/*************************************
**** ALGORITHMS
*************************************/
/*
	std::vector<int> in{ 1,2,3,4,5 };
	std::vector<int> out (2, 0);
	while (nextPermute(in, out)) {
		int m = out[0];
		int n = out[1];
*/
inline bool nextPermute(std::vector<int>& in, std::vector<int>& out) {

	size_t k = out.size();
	for (size_t i = 0; i < k; i++)
	{
		out[i] = in[i];
	}
	std::reverse(in.begin() + k, in.end());
	return std::next_permutation(in.begin(), in.end());
}

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

	void c_logger(const char* format, ...) {
		va_list args;
		va_start(args, format);

		vfprintf(stdout, format, args);
		fflush(stdout);  // Force a flush after printing

		va_end(args);
	}

#ifdef __cplusplus
} // extern "C"
#endif

