#include "auto-update-tooltip-label.hpp"

namespace advss {

AutoUpdateTooltipLabel::AutoUpdateTooltipLabel(
	QWidget *parent, const std::function<QString()> &updateTooltipCallback,
	int updateIntervalMs)
	: QLabel(parent),
	  _callback(updateTooltipCallback),
	  _timer(new QTimer(this)),
	  _updateIntervalMs(updateIntervalMs)
{
	connect(_timer, &QTimer::timeout, this,
		&AutoUpdateTooltipLabel::UpdateTooltip);
}

void AutoUpdateTooltipLabel::enterEvent(QEnterEvent *event)
{
	_timer->start(_updateIntervalMs);
	QLabel::enterEvent(event);
}

void AutoUpdateTooltipLabel::leaveEvent(QEvent *event)
{
	_timer->stop();
	QLabel::leaveEvent(event);
}

void AutoUpdateTooltipLabel::UpdateTooltip()
{
	setToolTip(_callback());
}

} // namespace advss
