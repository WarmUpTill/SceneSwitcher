#pragma once
#include "filter-combo-box.hpp"

namespace advss {

QStringList GetMonitorNames();

class MonitorSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	MonitorSelectionWidget(QWidget *parent);
	void SetUnavailableMonitor(const QString &name);

protected:
	void showEvent(QShowEvent *event) override;

private:
	QString _unavailableMonitor;
};

} // namespace advss
