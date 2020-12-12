#pragma once
#include "switch-generic.hpp"

constexpr auto read_file_func = 0;
constexpr auto default_priority_0 = read_file_func;

struct FileSwitch : SceneSwitcherEntry {
	std::string file;
	std::string text;
	bool remote = false;
	bool useRegex = false;
	bool useTime = false;
	bool onlyMatchIfChanged = false;
	QDateTime lastMod;
	size_t lastHash = 0;

	const char *getType() { return "file"; }

	inline FileSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  const char *file_, const char *text_, bool remote_,
			  bool useRegex_, bool useTime_,
			  bool onlyMatchIfChanged_)
		: SceneSwitcherEntry(scene_, transition_),
		  file(file_),
		  text(text_),
		  remote(remote_),
		  useRegex(useRegex_),
		  useTime(useTime_),
		  onlyMatchIfChanged(onlyMatchIfChanged_),
		  lastMod()
	{
	}
};

struct FileIOData {
	bool readEnabled = false;
	std::string readPath;
	bool writeEnabled = false;
	std::string writePath;
};

static inline QString MakeFileSwitchName(const QString &scene,
					 const QString &transition,
					 const QString &fileName,
					 const QString &text, bool useRegex,
					 bool useTime, bool onlyMatchIfChanged);
