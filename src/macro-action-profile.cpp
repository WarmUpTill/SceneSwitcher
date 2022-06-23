#include "headers/macro-action-profile.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionProfile::id = "profile";

bool MacroActionProfile::_registered = MacroActionFactory::Register(
	MacroActionProfile::id,
	{MacroActionProfile::Create, MacroActionProfileEdit::Create,
	 "AdvSceneSwitcher.action.profile"});

bool MacroActionProfile::PerformAction()
{
	obs_frontend_set_current_profile(_profile.c_str());
	return true;
}

void MacroActionProfile::LogAction()
{
	vblog(LOG_INFO, "set profile type to \"%s\"", _profile.c_str());
}

bool MacroActionProfile::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "profile", _profile.c_str());
	return true;
}

bool MacroActionProfile::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_profile = obs_data_get_string(obj, "profile");
	return true;
}

std::string MacroActionProfile::GetShortDesc()
{
	return _profile;
}

MacroActionProfileEdit::MacroActionProfileEdit(
	QWidget *parent, std::shared_ptr<MacroActionProfile> entryData)
	: QWidget(parent)
{
	_profiles = new QComboBox();
	populateProfileSelection(_profiles);
	QWidget::connect(_profiles, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(ProfileChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{profiles}}", _profiles},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.profile.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionProfileEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_profiles->setCurrentText(QString::fromStdString(_entryData->_profile));
}

void MacroActionProfileEdit::ProfileChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_profile = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}
