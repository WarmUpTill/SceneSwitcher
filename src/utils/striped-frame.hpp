#pragma once
#include <QFrame>
#include <QPainter>

class StripedFrame : public QFrame {
public:
	StripedFrame(QWidget *parent = nullptr) : QFrame(parent) {}

protected:
	void paintEvent(QPaintEvent *event) override;
};
