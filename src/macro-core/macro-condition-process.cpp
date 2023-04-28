#include "macro-condition-edit.hpp"
#include "macro-condition-process.hpp"
#include "platform-funcs.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

#include <regex>

namespace advss {

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

	if (IsReferencedInVars()) {
		std::string name;
		GetForegroundProcessName(name);
		SetVariableValue(name);
	}

	return (equals || matches) && focus;
}

bool MacroConditionProcess::Save(obs_data_t *obj) const
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

std::string MacroConditionProcess::GetShortDesc() const
{
	return _process;
}

MacroConditionProcessEdit::MacroConditionProcessEdit(
	QWidget *parent, std::shared_ptr<MacroConditionProcess> entryData)
	: QWidget(parent),
	  _processSelection(new QComboBox()),
	  _focused(new QCheckBox()),
	  _focusProcess(new QLabel()),
	  _focusLayout(new QHBoxLayout())
{
	_processSelection->setEditable(true);
	_processSelection->setMaxVisibleItems(20);

	QWidget::connect(_processSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ProcessChanged(const QString &)));
	QWidget::connect(_focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));
	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdateFocusProcess()));

	populateProcessSelection(_processSelection);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{processes}}", _processSelection},
		{"{{focused}}", _focused},
		{"{{focusProcess}}", _focusProcess},
	};

	auto entryLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.process.entry"),
		entryLayout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.process.entry.focus"),
		     _focusLayout, widgetPlaceholders);
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_focusLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	_timer.start(1000);
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
	SetWidgetVisibility();
}

void MacroConditionProcessEdit::UpdateFocusProcess()
{
	_focusProcess->setText(
		QString::fromStdString(switcher->currentForegroundProcess));
}

void MacroConditionProcessEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	setLayoutVisible(_focusLayout, _entryData->_focus);
	adjustSize();
}

void MacroConditionProcessEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_processSelection->setCurrentText(_entryData->_process.c_str());
	_focused->setChecked(_entryData->_focus);
	SetWidgetVisibility();
}

} // namespace advss
