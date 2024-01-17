#pragma once
#include <QWidget>

namespace advss {

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};

void PreventMouseWheelAdjustWithoutFocus(QWidget *);

} // namespace advss
