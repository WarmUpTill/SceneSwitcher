#include "twitch-timestamp.hpp"

#include <log-helper.hpp>

#include <chrono>
#include <ctime>
#include <regex>
#include <string>

using namespace std::chrono_literals;

namespace advss {

bool IsValidEventSubTimestamp(const std::string &timestamp)
{
	// Example input: 2023-07-19T14:56:51.634234626Z
	//            or: 2023-07-19T14:56:51+05:30
	static const std::regex pattern(
		R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?(Z|[+-]\d{2}:\d{2})$)");

	std::smatch m;
	if (!std::regex_match(timestamp, m, pattern)) {
		blog(LOG_WARNING, "failed to parse timestamp %s",
		     timestamp.c_str());
		return false;
	}

	std::tm tm = {};
	tm.tm_year = std::stoi(m[1]) - 1900;
	tm.tm_mon = std::stoi(m[2]) - 1;
	tm.tm_mday = std::stoi(m[3]);
	tm.tm_hour = std::stoi(m[4]);
	tm.tm_min = std::stoi(m[5]);
	tm.tm_sec = std::stoi(m[6]);

	// Range checks; timegm/_mkgmtime silently normalize out-of-range
	// fields instead of failing, so we need to catch that ourselves.
	if (tm.tm_mon < 0 || tm.tm_mon > 11 || tm.tm_mday < 1 ||
	    tm.tm_mday > 31 || tm.tm_hour > 23 || tm.tm_min > 59 ||
	    tm.tm_sec > 60 /* allow leap second */) {
		blog(LOG_WARNING, "timestamp field out of range %s",
		     timestamp.c_str());
		return false;
	}

#ifdef _WIN32
	time_t t = _mkgmtime(&tm);
#else
	time_t t = timegm(&tm);
#endif
	if (t == -1) {
		blog(LOG_WARNING, "failed to convert timestamp %s",
		     timestamp.c_str());
		return false;
	}

	// Reject dates that normalized to something else (e.g. Feb 30 -> Mar 2)
	if (tm.tm_mday != std::stoi(m[3]) || tm.tm_mon != std::stoi(m[2]) - 1) {
		blog(LOG_WARNING,
		     "timestamp date invalid after normalization %s",
		     timestamp.c_str());
		return false;
	}

	const std::string &tz = m[7];
	if (tz != "Z") {
		int offSign = (tz[0] == '+') ? 1 : -1;
		int offH = std::stoi(tz.substr(1, 2));
		int offM = std::stoi(tz.substr(4, 2));
		if (offH > 23 || offM > 59) {
			blog(LOG_WARNING, "invalid timestamp offset %s",
			     timestamp.c_str());
			return false;
		}
		time_t offsetSecs = (offH * 60 + offM) * 60;
		t += (offSign > 0) ? -offsetSecs : offsetSecs;
	}

	auto parsedTime = std::chrono::system_clock::from_time_t(t);
	auto now = std::chrono::system_clock::now();
	auto duration = now - parsedTime;
	// Clocks might be off by a bit, so allow negative values also
	return duration <= 10min && duration >= -1min;
}

} // namespace advss
