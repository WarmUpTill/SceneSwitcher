#include "striped-frame.hpp"

#include <cmath>

constexpr QColor stripeColor = QColor(255, 255, 255, 50);
constexpr int stripeWidth = 30;
constexpr int stripeSpacing = 30;
constexpr qreal sqrtOf2 = 1.41421356237309;
constexpr int painterVerticalOffset = stripeWidth / sqrtOf2 + 1;
constexpr int rotatedPainterSpacing =
	(stripeWidth + stripeSpacing) / sqrtOf2 + 1;
constexpr qreal horizontalMoveStep = stripeWidth + stripeSpacing;
constexpr qreal verticalMoveStep = -horizontalMoveStep;

void StripedFrame::paintEvent(QPaintEvent *event)
{
	QFrame::paintEvent(event);

	QPainter painter(this);
	painter.setPen(Qt::NoPen);
	painter.setBrush(stripeColor);

	const int biggerSideSize = width() > height() ? width() : height();
	const int numStripes = biggerSideSize / rotatedPainterSpacing + 1;

	painter.translate(0, -painterVerticalOffset);
	painter.rotate(45);

	const qreal diagonalLength =
		std::sqrt(width() * width() + height() * height());
	const qreal stripeLength = diagonalLength * 2;

	for (int i = 0; i < numStripes; ++i) {
		painter.drawRect(0, 0, stripeWidth, stripeLength);
		painter.translate(horizontalMoveStep, verticalMoveStep);
	}
}
