#include "profile-helpers.hpp"
#include "obs-module-helper.hpp"
#include "selection-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

void PopulateProfileSelection(QComboBox *box)
{
	auto profiles = obs_frontend_get_profiles();
	char **temp = profiles;
	while (*temp) {
		const char *name = *temp;
		box->addItem(name);
		temp++;
	}
	bfree(profiles);
	box->model()->sort(0);
	AddSelectionEntry(
		box, obs_module_text("AdvSceneSwitcher.selectProfile"), false);
	box->setCurrentIndex(0);
}

std::string GetPathInProfileDir(const char *filePath)
{
	auto path = obs_frontend_get_current_profile_path();
	std::string result(path);
	bfree(path);
	return result + "/" + filePath;
}

ProfileSelectionWidget::ProfileSelectionWidget(QWidget *parent)
	: FilterComboBox(parent)
{
	setEditable(true);
	SetAllowUnmatchedSelection(true);
	setMaxVisibleItems(20);
	PopulateProfileSelection(this);
}

void ProfileSelectionWidget::showEvent(QShowEvent *event)
{
	FilterComboBox::showEvent(event);
	const QSignalBlocker b(this);
	const auto text = currentText();
	clear();
	PopulateProfileSelection(this);
	setCurrentText(text);
}

} // namespace advss
