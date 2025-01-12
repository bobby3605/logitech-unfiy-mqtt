#include "common.hpp"
#include <ctime>

std::string curr_time() { 
	std::time_t sys_time = std::time(nullptr);
	tm local_time; 
	localtime_s(&local_time, &sys_time);
	char buffer[80];
	strftime(buffer, 80, "[%c]", &local_time);
	return std::string(buffer) + " ";
}
