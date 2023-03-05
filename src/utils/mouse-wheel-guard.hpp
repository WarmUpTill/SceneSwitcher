#pragma once
#include <QWidget>

class MouseWheelWidgetAdjustmentGuard : public QObject {
public:
	explicit MouseWheelWidgetAdjustmentGuard(QObject *parent);

protected:
	bool eventFilter(QObject *o, QEvent *e) override;
};

void PreventMouseWheelAdjustWithoutFocus(QWidget *);
