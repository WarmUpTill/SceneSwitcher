#pragma once
#include <obs.hpp>
#include <string>
#include <QImage>
#include <chrono>
#include <thread>

class ScreenshotHelper {
public:
	ScreenshotHelper() = default;
	ScreenshotHelper(obs_source_t *source, bool saveToFile = false,
			 std::string path = "");
	ScreenshotHelper &operator=(const ScreenshotHelper &) = delete;
	ScreenshotHelper(const ScreenshotHelper &) = delete;
	~ScreenshotHelper();

	void Screenshot();
	void Download();
	void Copy();
	void MarkDone();
	void WriteToFile();

	gs_texrender_t *texrender = nullptr;
	gs_stagesurf_t *stagesurf = nullptr;
	OBSWeakSource weakSource;
	QImage image;
	uint32_t cx = 0;
	uint32_t cy = 0;

	int stage = 0;

	bool done = false;
	std::chrono::high_resolution_clock::time_point time;

private:
	bool _initDone = false;
	std::thread _saveThread;
	bool _saveToFile = false;
	std::string _path = "";
};
