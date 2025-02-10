#pragma once
#include <chrono>

using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
using Milli = std::chrono::duration<double, std::milli>;
using Seconds = std::chrono::duration<double, std::ratio<1>>;

Seconds GEngineTime{ 0 };
//Time GNow;
//Time GGameStart;

Time getTime() {
	return std::chrono::high_resolution_clock::now();
}
