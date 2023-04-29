#pragma once
#include <vector>
#include <string>

#include <obs-data.h>

namespace advss {

struct ThreadPrio {
	std::string name;
	std::string description;
	uint32_t value;
};

std::vector<int> GetDefaultFunctionPriorityList();
void SetDefaultFunctionPriorities(obs_data_t *);
std::vector<ThreadPrio> GetThreadPrioMapping();

} // namespace advss
