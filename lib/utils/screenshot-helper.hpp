#pragma once
#include <obs.hpp>
#include <string>
#include <QImage>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace advss {

class Screenshot {
	using TimePoint = std::chrono::high_resolution_clock::time_point;

public:
	EXPORT Screenshot() = default;
	EXPORT Screenshot(obs_source_t *source, const QRect &subarea = QRect(),
			  bool blocking = false, int timeout = 1000,
			  bool saveToFile = false, std::string path = "");
	EXPORT Screenshot &operator=(const Screenshot &) = delete;
	EXPORT Screenshot(const Screenshot &) = delete;
	EXPORT ~Screenshot();

	EXPORT bool IsDone() const { return _done; }
	EXPORT QImage &GetImage() { return _image; }
	EXPORT TimePoint GetScreenshotTime() const { return _time; }

private:
	static void ScreenshotTick(void *param, float);
	void CreateScreenshot();
	void Download();
	void Copy();
	void MarkDone();
	void WriteToFile();

	gs_texrender_t *_texrender = nullptr;
	gs_stagesurf_t *_stagesurf = nullptr;
	OBSWeakSource _weakSource;
	QImage _image;
	uint32_t _cx = 0;
	uint32_t _cy = 0;

	int _stage = 0;

	bool _done = false;
	TimePoint _time;

	std::atomic_bool _initDone = false;
	QRect _subarea = QRect();
	bool _blocking = false;
	std::thread _saveThread;
	bool _saveToFile = false;
	std::string _path = "";
	std::mutex _mutex;
	std::condition_variable _cv;
};

} // namespace advss
