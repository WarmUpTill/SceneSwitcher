#include "priority-helper.hpp"
#include "scene-group.hpp"
#include "scene-trigger.hpp"
#include "switch-audio.hpp"
#include "switch-executable.hpp"
#include "switch-file.hpp"
#include "switch-idle.hpp"
#include "switch-media.hpp"
#include "switch-pause.hpp"
#include "switch-random.hpp"
#include "switch-screen-region.hpp"
#include "switch-time.hpp"
#include "switch-transitions.hpp"
#include "switch-window.hpp"
#include "switch-sequence.hpp"
#include "switch-video.hpp"
#include "switch-network.hpp"
#include "macro.hpp"

#include <QThread>

namespace advss {

constexpr auto default_priority_0 = macro_func;
constexpr auto default_priority_1 = read_file_func;
constexpr auto default_priority_2 = idle_func;
constexpr auto default_priority_3 = audio_func;
constexpr auto default_priority_4 = media_func;
constexpr auto default_priority_5 = video_func;
constexpr auto default_priority_6 = time_func;
constexpr auto default_priority_7 = screen_region_func;
constexpr auto default_priority_8 = round_trip_func;
constexpr auto default_priority_9 = window_title_func;
constexpr auto default_priority_10 = exe_func;

void SetDefaultFunctionPriorities(obs_data_t *obj)
{
	obs_data_set_default_int(obj, "priority0", default_priority_0);
	obs_data_set_default_int(obj, "priority1", default_priority_1);
	obs_data_set_default_int(obj, "priority2", default_priority_2);
	obs_data_set_default_int(obj, "priority3", default_priority_3);
	obs_data_set_default_int(obj, "priority4", default_priority_4);
	obs_data_set_default_int(obj, "priority5", default_priority_5);
	obs_data_set_default_int(obj, "priority6", default_priority_6);
	obs_data_set_default_int(obj, "priority7", default_priority_7);
	obs_data_set_default_int(obj, "priority8", default_priority_8);
	obs_data_set_default_int(obj, "priority9", default_priority_9);
	obs_data_set_default_int(obj, "priority10", default_priority_10);
}

std::vector<int> GetDefaultFunctionPriorityList()
{
	return {default_priority_0, default_priority_1, default_priority_2,
		default_priority_3, default_priority_4, default_priority_5,
		default_priority_6, default_priority_7, default_priority_8,
		default_priority_9, default_priority_10};
}

std::vector<ThreadPrio> GetThreadPrioMapping()
{
	return {
		{"Idle",
		 "scheduled only when no other threads are running (lowest CPU load)",
		 QThread::IdlePriority},
		{"Lowest", "scheduled less often than LowPriority",
		 QThread::LowestPriority},
		{"Low", "scheduled less often than NormalPriority",
		 QThread::LowPriority},
		{"Normal", "the default priority of the operating system",
		 QThread::NormalPriority},
		{"High", "scheduled more often than NormalPriority",
		 QThread::HighPriority},
		{"Highest", "scheduled more often than HighPriority",
		 QThread::HighestPriority},
		{"Time critical",
		 "scheduled as often as possible (highest CPU load)",
		 QThread::TimeCriticalPriority},
	};
}

} // namespace advss
