#pragma once
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

namespace advss {

class ResizableWidget : public QWidget {
	Q_OBJECT

public:
	ResizableWidget(QWidget *parent = nullptr);

protected:
	void SetResizingEnabled(bool enable);
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	QSize sizeHint() const override;
	int GetCustomHeight() const { return _customHeight; }
	void SetCustomHeight(int value) { _customHeight = value; }

private:
	bool IsInResizeArea(const QPoint &pos) const;

	bool _resizingEnabled = false;
	bool _resizing = false;
	QPoint _lastMousePos;
	int _customHeight = 0;
	const int _gripSize = 15;
};

} // namespace advss
