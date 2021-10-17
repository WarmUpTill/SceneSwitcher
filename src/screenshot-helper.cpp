#include "headers/screenshot-helper.hpp"
#include "headers/advanced-scene-switcher.hpp"

static void ScreenshotTick(void *param, float);

AdvSSScreenshotObj::AdvSSScreenshotObj(obs_source_t *source)
	: weakSource(OBSGetWeakRef(source))
{
	obs_add_tick_callback(ScreenshotTick, this);
}

AdvSSScreenshotObj::~AdvSSScreenshotObj()
{
	obs_enter_graphics();
	gs_stagesurface_destroy(stagesurf);
	gs_texrender_destroy(texrender);
	obs_leave_graphics();

	obs_remove_tick_callback(ScreenshotTick, this);
}

void AdvSSScreenshotObj::Screenshot()
{
	OBSSource source = OBSGetStrongRef(weakSource);

	if (source) {
		cx = obs_source_get_base_width(source);
		cy = obs_source_get_base_height(source);
	} else {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		cx = ovi.base_width;
		cy = ovi.base_height;
	}

	if (!cx || !cy) {
		vblog(LOG_WARNING,
		      "Cannot screenshot \"%s\", invalid target size",
		      obs_source_get_name(source));
		obs_remove_tick_callback(ScreenshotTick, this);
		done = true;
		return;
	}

	texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	stagesurf = gs_stagesurface_create(cx, cy, GS_RGBA);

	gs_texrender_reset(texrender);
	if (gs_texrender_begin(texrender, cx, cy)) {
		vec4 zero;
		vec4_zero(&zero);

		gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

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
		gs_texrender_end(texrender);
	}
}

void AdvSSScreenshotObj::Download()
{
	gs_stage_texture(stagesurf, gs_texrender_get_texture(texrender));
}

void AdvSSScreenshotObj::Copy()
{
	uint8_t *videoData = nullptr;
	uint32_t videoLinesize = 0;

	image = QImage(cx, cy, QImage::Format::Format_RGBA8888);

	if (gs_stagesurface_map(stagesurf, &videoData, &videoLinesize)) {
		int linesize = image.bytesPerLine();
		for (int y = 0; y < (int)cy; y++)
			memcpy(image.scanLine(y),
			       videoData + (y * videoLinesize), linesize);

		gs_stagesurface_unmap(stagesurf);
	}
}

void AdvSSScreenshotObj::MarkDone()
{
	time = std::chrono::high_resolution_clock::now();
	done = true;
}

#define STAGE_SCREENSHOT 0
#define STAGE_DOWNLOAD 1
#define STAGE_COPY_AND_SAVE 2
#define STAGE_FINISH 3

static void ScreenshotTick(void *param, float)
{
	AdvSSScreenshotObj *data =
		reinterpret_cast<AdvSSScreenshotObj *>(param);

	if (data->stage == STAGE_FINISH) {
		return;
	}

	obs_enter_graphics();

	switch (data->stage) {
	case STAGE_SCREENSHOT:
		data->Screenshot();
		break;
	case STAGE_DOWNLOAD:
		data->Download();
		break;
	case STAGE_COPY_AND_SAVE:
		data->Copy();
		data->MarkDone();

		obs_remove_tick_callback(ScreenshotTick, data);
		break;
	}

	obs_leave_graphics();

	data->stage++;
}
