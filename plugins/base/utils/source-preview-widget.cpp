#include "source-preview-widget.hpp"

#include <obs.hpp>
#include <graphics/graphics.h>

#include <QResizeEvent>

namespace advss {

static int32_t clampToSource(int32_t v, uint32_t max)
{
	if (v < 0) {
		return 0;
	}
	if (v > (int32_t)max) {
		return (int32_t)max;
	}
	return v;
}

SourcePreviewWidget::SourcePreviewWidget(QWidget *parent,
					 obs_weak_source_t *source)
	: QWidget(parent),
	  _source(source)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(320, 240);
}

SourcePreviewWidget::~SourcePreviewWidget()
{
	if (_display) {
		obs_display_remove_draw_callback(
			_display, SourcePreviewWidget::DrawCallback, this);
		obs_display_destroy(_display);
		_display = nullptr;
	}

	if (_showing) {
		OBSSourceAutoRelease source =
			obs_weak_source_get_source(_source);
		if (source) {
			obs_source_dec_showing(source);
		}
		_showing = false;
	}
}

void SourcePreviewWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	if (!_display) {
		WId wid = winId();
		if (!wid) {
			return;
		}

		gs_init_data info{};
#if defined(_WIN32)
		info.window.hwnd = reinterpret_cast<void *>(wid);
#elif defined(__APPLE__)
		info.window.view = reinterpret_cast<id>(wid);
#else
		info.window.id = static_cast<uint32_t>(wid);
		info.window.display = nullptr;
#endif
		info.format = GS_BGRA;
		info.zsformat = GS_ZS_NONE;
		info.cx = static_cast<uint32_t>(width());
		info.cy = static_cast<uint32_t>(height());

		_display = obs_display_create(&info, 0x000000);
		if (_display) {
			obs_display_add_draw_callback(
				_display, SourcePreviewWidget::DrawCallback,
				this);
			OBSSourceAutoRelease source =
				obs_weak_source_get_source(_source);
			if (source) {
				obs_source_inc_showing(source);
				_showing = true;
			}
		}
	} else {
		obs_display_resize(_display, static_cast<uint32_t>(width()),
				   static_cast<uint32_t>(height()));
	}
}

void SourcePreviewWidget::DrawCallback(void *param, uint32_t cx, uint32_t cy)
{
	auto self = static_cast<SourcePreviewWidget *>(param);

	OBSSourceAutoRelease source = obs_weak_source_get_source(self->_source);
	if (!source) {
		return;
	}

	uint32_t srcW = std::max(obs_source_get_width(source), 1u);
	uint32_t srcH = std::max(obs_source_get_height(source), 1u);

	float scaleX = static_cast<float>(cx) / srcW;
	float scaleY = static_cast<float>(cy) / srcH;
	float scale = std::min(scaleX, scaleY);

	int newW = static_cast<int>(scale * srcW);
	int newH = static_cast<int>(scale * srcH);
	int offX = (static_cast<int>(cx) - newW) / 2;
	int offY = (static_cast<int>(cy) - newH) / 2;

	self->_offsetX = offX;
	self->_offsetY = offY;
	self->_scale = scale;

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, static_cast<float>(srcW), 0.0f, static_cast<float>(srcH),
		 -100.0f, 100.0f);
	gs_set_viewport(offX, offY, newW, newH);

	obs_source_video_render(source);

	gs_projection_pop();
	gs_viewport_pop();
}

bool SourcePreviewWidget::GetSourceRelativeXY(int widgetX, int widgetY,
					      int &srcX, int &srcY) const
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(_source);
	if (!source) {
		return false;
	}

	uint32_t srcW = std::max(obs_source_get_width(source), 1u);
	uint32_t srcH = std::max(obs_source_get_height(source), 1u);

	srcX = static_cast<int>((widgetX - _offsetX) / _scale);
	srcY = static_cast<int>((widgetY - _offsetY) / _scale);
	srcX = static_cast<int>(
		clampToSource(static_cast<int32_t>(srcX), srcW));
	srcY = static_cast<int>(
		clampToSource(static_cast<int32_t>(srcY), srcH));
	return true;
}

} // namespace advss
