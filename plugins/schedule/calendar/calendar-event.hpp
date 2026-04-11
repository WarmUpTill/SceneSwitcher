#pragma once

#include <QColor>
#include <QDateTime>
#include <QString>
#include <QVariant>

namespace advss {

// Generic calendar event. All fields except id/start are optional.
struct CalendarEvent {
	QString id;
	QString title;
	QDateTime start;
	QDateTime end;
	QColor color{70, 130, 180};
	QVariant userData; // Caller-defined payload, returned on click signals

	// Returns end if valid and > start, otherwise start + 30 minutes.
	QDateTime EffectiveEnd() const
	{
		static constexpr qint64 defaultDuration = 1800;
		return (end.isValid() && end > start)
			       ? end
			       : start.addSecs(defaultDuration);
	}
};

} // namespace advss
