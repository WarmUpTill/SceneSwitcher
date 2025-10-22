#include "auto-update-tooltip-label.hpp"

namespace advss {

AutoUpdateHelpIcon::AutoUpdateHelpIcon(
	QWidget *parent, const std::function<QString()> &updateTooltipCallback,
	int updateIntervalMs)
	: HelpIcon("", parent),
	  _callback(updateTooltipCallback),
	  _timer(new QTimer(this)),
	  _updateIntervalMs(updateIntervalMs)
{
	connect(_timer, &QTimer::timeout, this,
		&AutoUpdateHelpIcon::UpdateTooltip);
}

void AutoUpdateHelpIcon::enterEvent(QEnterEvent *event)
{
	_timer->start(_updateIntervalMs);
	QLabel::enterEvent(event);
}

void AutoUpdateHelpIcon::leaveEvent(QEvent *event)
{
	_timer->stop();
	QLabel::leaveEvent(event);
}

void AutoUpdateHelpIcon::UpdateTooltip()
{
	setToolTip(_callback());
}

} // namespace advss
