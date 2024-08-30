#pragma once
#include <QWidget>

namespace advss {

class CursorShapeChanger : public QObject {
public:
	CursorShapeChanger(QObject *, Qt::CursorShape shape);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;

private:
	Qt::CursorShape _shape;
	std::atomic_bool _overrideActive = {false};
};

void SetCursorOnWidgetHover(QWidget *widget, Qt::CursorShape shape);

} // namespace advss
