#pragma once
#include <QFrame>
#include <QPainter>

namespace advss {

class StripedFrame : public QFrame {
public:
	StripedFrame(QWidget *parent = nullptr) : QFrame(parent) {}

protected:
	void paintEvent(QPaintEvent *event) override;
};

} // namespace advss
