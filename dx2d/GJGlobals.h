#pragma once
#include <chrono>

std::chrono::time_point<std::chrono::high_resolution_clock> GNow;
std::chrono::time_point<std::chrono::high_resolution_clock> GGameStart;

auto getTime() {
	return std::chrono::high_resolution_clock::now();
}
