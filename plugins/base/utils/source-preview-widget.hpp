#pragma once

#include <obs.h>
#include <graphics/graphics.h>

#include <QWidget>

class QResizeEvent;

namespace advss {

// A native window widget that renders an OBS source via obs_display.
// Stores letterbox geometry for coordinate mapping.
class SourcePreviewWidget : public QWidget {
	Q_OBJECT
public:
	SourcePreviewWidget(QWidget *parent, obs_weak_source_t *source);
	~SourcePreviewWidget();

	// Maps a widget-local point to source-space coordinates.
	bool GetSourceRelativeXY(int widgetX, int widgetY, int &srcX,
				 int &srcY) const;

protected:
	void resizeEvent(QResizeEvent *) override;
	QPaintEngine *paintEngine() const override { return nullptr; }

private:
	static void DrawCallback(void *param, uint32_t cx, uint32_t cy);

	obs_weak_source_t *_source;
	obs_display_t *_display = nullptr;
	bool _showing = false;

	int _offsetX = 0;
	int _offsetY = 0;
	float _scale = 1.0f;
};

} // namespace advss
