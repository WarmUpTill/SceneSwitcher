#pragma once
#include <string>
#include "utility.hpp"

constexpr auto read_file_func = 0;
constexpr auto default_priority_0 = read_file_func;

struct FileSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	std::string file;
	std::string text;
	bool remote = false;
	bool useRegex = false;
	bool useTime = false;
	QDateTime lastMod;

	bool valid()
	{
		return WeakSourceValid(scene) && WeakSourceValid(transition);
	}

	inline FileSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  const char *file_, const char *text_, bool remote_,
			  bool useRegex_, bool useTime_)
		: scene(scene_),
		  transition(transition_),
		  file(file_),
		  text(text_),
		  remote(remote_),
		  useRegex(useRegex_),
		  useTime(useTime_),
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
