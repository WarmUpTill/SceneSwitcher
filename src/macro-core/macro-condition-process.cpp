#include "macro-condition-edit.hpp"
#include "macro-condition-process.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionProcess::id = "process";

bool MacroConditionProcess::_registered = MacroConditionFactory::Register(
	MacroConditionProcess::id,
	{MacroConditionProcess::Create, MacroConditionProcessEdit::Create,
	 "AdvSceneSwitcher.condition.process"});

bool MacroConditionProcess::CheckCondition()
{
	QStringList runningProcesses;
	QString proc = QString::fromStdString(_process);
	GetProcessList(runningProcesses);

	bool equals = runningProcesses.contains(proc);
	bool matches = runningProcesses.indexOf(QRegularExpression(proc)) != -1;
	bool focus = !_focus || isInFocus(proc);

	return (equals || matches) && focus;
}

bool MacroConditionProcess::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "process", _process.c_str());
	obs_data_set_bool(obj, "focus", _focus);
	return true;
}

bool MacroConditionProcess::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_process = obs_data_get_string(obj, "process");
	_focus = obs_data_get_bool(obj, "focus");
	return true;
}

std::string MacroConditionProcess::GetShortDesc()
{
	return _process;
}

MacroConditionProcessEdit::MacroConditionProcessEdit(
	QWidget *parent, std::shared_ptr<MacroConditionProcess> entryData)
	: QWidget(parent)
{
	_processSelection = new QComboBox();
	_processSelection->setEditable(true);
	_processSelection->setMaxVisibleItems(20);

	_focused = new QCheckBox();

	QWidget::connect(_processSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ProcessChanged(const QString &)));
	QWidget::connect(_focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));

	populateProcessSelection(_processSelection);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{processes}}", _processSelection},
		{"{{focused}}", _focused},
	};

	QHBoxLayout *mainLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.process.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionProcessEdit::ProcessChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_process = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionProcessEdit::FocusChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_focus = state;
}

void MacroConditionProcessEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_processSelection->setCurrentText(_entryData->_process.c_str());
	_focused->setChecked(_entryData->_focus);
}
