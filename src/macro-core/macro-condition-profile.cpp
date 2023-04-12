#include "macro-condition-edit.hpp"
#include "macro-condition-profile.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

namespace advss {

const std::string MacroConditionProfile::id = "profile";

bool MacroConditionProfile::_registered = MacroConditionFactory::Register(
	MacroConditionProfile::id,
	{MacroConditionProfile::Create, MacroConditionProfileEdit::Create,
	 "AdvSceneSwitcher.condition.profile"});

bool MacroConditionProfile::CheckCondition()
{
	auto currentProfile = obs_frontend_get_current_profile();
	const bool match = _profile == currentProfile;
	bfree(currentProfile);
	return match;
}

bool MacroConditionProfile::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "profile", _profile.c_str());
	return true;
}

bool MacroConditionProfile::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_profile = obs_data_get_string(obj, "profile");
	return true;
}

std::string MacroConditionProfile::GetShortDesc() const
{
	return _profile;
}

MacroConditionProfileEdit::MacroConditionProfileEdit(
	QWidget *parent, std::shared_ptr<MacroConditionProfile> entryData)
	: QWidget(parent), _profiles(new QComboBox())
{
	populateProfileSelection(_profiles);
	QWidget::connect(_profiles, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(ProfileChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{profiles}}", _profiles},
	};

	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.profile.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionProfileEdit::ProfileChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_profile = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionProfileEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_profiles->setCurrentText(QString::fromStdString(_entryData->_profile));
}

} // namespace advss
