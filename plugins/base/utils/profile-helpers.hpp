#pragma once
#include "filter-combo-box.hpp"

#include <string>

namespace advss {

void PopulateProfileSelection(QComboBox *list);
std::string GetPathInProfileDir(const char *filePath);

class ProfileSelectionWidget : public FilterComboBox {
	Q_OBJECT

public:
	ProfileSelectionWidget(QWidget *parent);

protected:
	void showEvent(QShowEvent *event) override;
};

} // namespace advss
