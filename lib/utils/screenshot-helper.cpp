#include "screenshot-helper.hpp"
#include "advanced-scene-switcher.hpp"

#include <chrono>

namespace advss {

Screenshot::Screenshot(obs_source_t *source, const QRect &subarea,
		       bool blocking, int timeout, bool saveToFile,
		       std::string path)
	: _weakSource(OBSGetWeakRef(source)),
	  _subarea(subarea),
	  _blocking(blocking),
	  _saveToFile(saveToFile),
	  _path(path)
{
	std::unique_lock<std::mutex> lock(_mutex);
	_initDone = true;
	obs_add_tick_callback(ScreenshotTick, this);
	if (_blocking) {
		auto res =
			_cv.wait_for(lock, std::chrono::milliseconds(timeout));
		if (res == std::cv_status::timeout) {
			if (source) {
				blog(LOG_WARNING,
				     "Failed to get screenshot in time for source %s",
				     obs_source_get_name(source));
			} else {
				blog(LOG_WARNING,
				     "Failed to get screenshot in time");
			}
		}
	}
}

Screenshot::~Screenshot()
{
	if (_initDone) {
		obs_enter_graphics();
		gs_stagesurface_destroy(_stagesurf);
		gs_texrender_destroy(_texrender);
		obs_leave_graphics();
	}
	obs_remove_tick_callback(ScreenshotTick, this);
	if (_saveThread.joinable()) {
		_saveThread.join();
	}
}

void Screenshot::CreateScreenshot()
{
	OBSSource source = OBSGetStrongRef(_weakSource);

	if (source) {
		_cx = obs_source_get_base_width(source);
		_cy = obs_source_get_base_height(source);
	} else {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		_cx = ovi.base_width;
		_cy = ovi.base_height;
	}

	QRect renderArea(0, 0, _cx, _cy);
	if (!_subarea.isEmpty()) {
		renderArea &= _subarea;
	}

	if (renderArea.isEmpty()) {
		vblog(LOG_WARNING,
		      "Cannot screenshot \"%s\", invalid target size",
		      obs_source_get_name(source));
		obs_remove_tick_callback(ScreenshotTick, this);
		_done = true;
		return;
	}

	_cx = renderArea.width();
	_cy = renderArea.height();

	_texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	_stagesurf = gs_stagesurface_create(renderArea.width(),
					    renderArea.height(), GS_RGBA);

	gs_texrender_reset(_texrender);
	if (gs_texrender_begin(_texrender, renderArea.width(),
			       renderArea.height())) {
		vec4 zero;
		vec4_zero(&zero);

		gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
		gs_ortho((float)(renderArea.left()),
			 (float)(renderArea.right() + 1),
			 (float)(renderArea.top()),
			 (float)(renderArea.bottom() + 1), -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		if (source) {
			obs_source_inc_showing(source);
			obs_source_video_render(source);
			obs_source_dec_showing(source);
		} else {
			obs_render_main_texture();
		}

		gs_blend_state_pop();
		gs_texrender_end(_texrender);
	}
}

void Screenshot::Download()
{
	gs_stage_texture(_stagesurf, gs_texrender_get_texture(_texrender));
}

void Screenshot::Copy()
{
	uint8_t *videoData = nullptr;
	uint32_t videoLinesize = 0;

	_image = QImage(_cx, _cy, QImage::Format::Format_RGBA8888);

	if (gs_stagesurface_map(_stagesurf, &videoData, &videoLinesize)) {
		int linesize = _image.bytesPerLine();
		for (int y = 0; y < (int)_cy; y++)
			memcpy(_image.scanLine(y),
			       videoData + (y * videoLinesize), linesize);

		gs_stagesurface_unmap(_stagesurf);
	}
}

void Screenshot::MarkDone()
{
	_time = std::chrono::high_resolution_clock::now();
	_done = true;
	std::unique_lock<std::mutex> lock(_mutex);
	_cv.notify_all();
}

void Screenshot::WriteToFile()
{
	if (!_saveToFile) {
		return;
	}

	_saveThread = std::thread([this]() {
		if (_image.save(QString::fromStdString(_path))) {
			vblog(LOG_INFO, "Wrote screenshot to \"%s\"",
			      _path.c_str());
		} else {
			blog(LOG_WARNING,
			     "Failed to save screenshot to \"%s\"!\nMaybe unknown format?",
			     _path.c_str());
		}
	});
}

#define STAGE_SCREENSHOT 0
#define STAGE_DOWNLOAD 1
#define STAGE_COPY_AND_SAVE 2
#define STAGE_FINISH 3

void Screenshot::ScreenshotTick(void *param, float)
{
	Screenshot *data = reinterpret_cast<Screenshot *>(param);

	if (data->_stage == STAGE_FINISH) {
		return;
	}

	obs_enter_graphics();

	switch (data->_stage) {
	case STAGE_SCREENSHOT:
		data->CreateScreenshot();
		break;
	case STAGE_DOWNLOAD:
		data->Download();
		break;
	case STAGE_COPY_AND_SAVE:
		data->Copy();
		data->WriteToFile();
		data->MarkDone();

		obs_remove_tick_callback(ScreenshotTick, data);
		break;
	}

	obs_leave_graphics();

	data->_stage++;
}

} // namespace advss
