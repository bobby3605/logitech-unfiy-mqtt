#pragma once
// magically fix static linking
#pragma comment(lib, "crypt32")
#pragma comment(lib, "ws2_32.lib")
#include <thread>
#include <string>
std::string curr_time();
