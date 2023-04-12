#include "macro-condition-edit.hpp"
#include "macro-condition-hotkey.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

namespace advss {

const std::string MacroConditionHotkey::id = "hotkey";

bool MacroConditionHotkey::_registered = MacroConditionFactory::Register(
	MacroConditionHotkey::id,
	{MacroConditionHotkey::Create, MacroConditionHotkeyEdit::Create,
	 "AdvSceneSwitcher.condition.hotkey", false});

static uint32_t count = 1;

MacroConditionHotkey::MacroConditionHotkey(Macro *m) : MacroCondition(m)
{
	auto name = obs_module_text("AdvSceneSwitcher.condition.hotkey.name") +
		    std::string(" ") + std::to_string(count);
	_hotkey = Hotkey::GetHotkey(name, true);
	count++;
}

bool MacroConditionHotkey::CheckCondition()
{
	bool ret = _hotkey->GetPressed() ||
		   _hotkey->GetLastPressed() > _lastCheck;
	_lastCheck = std::chrono::high_resolution_clock::now();
	return ret;
}

bool MacroConditionHotkey::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_hotkey->Save(obj);
	return true;
}

bool MacroConditionHotkey::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	if (!_hotkey->Load(obj)) {
		auto description = obs_data_get_string(obj, "desc");
		_hotkey = Hotkey::GetHotkey(description);
		vblog(LOG_WARNING,
		      "hotkey name conflict for \"%s\" - using previous key bind",
		      description);
	}
	return true;
}

MacroConditionHotkeyEdit::MacroConditionHotkeyEdit(
	QWidget *parent, std::shared_ptr<MacroConditionHotkey> entryData)
	: QWidget(parent)
{
	_name = new QLineEdit();
	QLabel *line1 = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.hotkey.entry.line1"));
	QLabel *hint = new QLabel(
		obs_module_text("AdvSceneSwitcher.condition.hotkey.tip"));

	QWidget::connect(_name, SIGNAL(editingFinished()), this,
			 SLOT(NameChanged()));

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{name}}", _name},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.hotkey.entry.line2"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(line1);
	mainLayout->addLayout(switchLayout);
	mainLayout->addWidget(hint);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionHotkeyEdit::NameChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	const auto name = _name->text().toStdString();
	// In case a hotkey is used by multiple conditions create a new hotkey
	// with the new description or get an existing hotkey matching this
	// description.
	// If the hotkey is only used by this single condition instance try to
	// update the description.
	// If updating the description runs into a conflict with an existing
	// hotkey use its settings instead.
	if (_entryData->_hotkey.use_count() > 1 ||
	    !_entryData->_hotkey->UpdateDescription(name)) {
		_entryData->_hotkey = Hotkey::GetHotkey(name);
	}
}

void MacroConditionHotkeyEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_name->setText(
		QString::fromStdString(_entryData->_hotkey->GetDescription()));
}

} // namespace advss
