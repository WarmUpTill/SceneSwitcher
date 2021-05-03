#pragma once
#include <obs.hpp>
#include <string>
#include <QImage>
#include <chrono>

class AdvSSScreenshotObj {
public:
	AdvSSScreenshotObj(obs_source_t *source);
	~AdvSSScreenshotObj();

	void Screenshot();
	void Download();
	void Copy();
	void MarkDone();

	gs_texrender_t *texrender = nullptr;
	gs_stagesurf_t *stagesurf = nullptr;
	OBSWeakSource weakSource;
	std::string path;
	QImage image;
	uint32_t cx = 0;
	uint32_t cy = 0;

	int stage = 0;

	bool done = false;
	std::chrono::high_resolution_clock::time_point time;
};
