#pragma once
#include "export-symbol-helper.hpp"

#include <functional>
#include <QLabel>
#include <QTimer>

namespace advss {

class ADVSS_EXPORT AutoUpdateTooltipLabel : public QLabel {
	Q_OBJECT

public:
	AutoUpdateTooltipLabel(
		QWidget *parent,
		const std::function<QString()> &updateTooltipCallback,
		int updateIntervalMs = 300);

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

private slots:
	void UpdateTooltip();

private:
	const std::function<QString()> _callback;
	QTimer *_timer;
	const int _updateIntervalMs;
};

} // namespace advss
