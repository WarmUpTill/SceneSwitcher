#pragma once
#include "filter-combo-box.hpp"

namespace advss {

class WindowSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	WindowSelectionWidget(QWidget *parent);

protected:
	void showEvent(QShowEvent *event) override;
};

} // namespace advss
