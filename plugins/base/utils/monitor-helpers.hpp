#pragma once
#include "filter-combo-box.hpp"

namespace advss {

QStringList GetMonitorNames();

class MonitorSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	MonitorSelectionWidget(QWidget *parent);

protected:
	void showEvent(QShowEvent *event) override;
};

} // namespace advss
