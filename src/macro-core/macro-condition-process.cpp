#include "macro-condition-process.hpp"
#include "platform-funcs.hpp"
#include "utility.hpp"

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
	std::string foregroundProcessName;
	GetForegroundProcessName(foregroundProcessName);

	SetVariableValue(foregroundProcessName);

	if (!_regex.Enabled()) {
		if (runningProcesses.contains(proc) &&
		    (!_checkFocus || IsInFocus(proc))) {
			SetTempVarValue("name", proc.toStdString());
			return true;
		}
		return false;
	}

	auto matchIndex = runningProcesses.indexOf(QRegularExpression(proc));
	if (matchIndex == -1) {
		return false;
	}
	if (!_checkFocus) {
		SetTempVarValue("name",
				runningProcesses.at(matchIndex).toStdString());
		return true;
	}
	if (!IsInFocus(proc)) {
		return false;
	}
	SetTempVarValue("name", foregroundProcessName);
	return true;
}

bool MacroConditionProcess::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "process", _process.c_str());
	obs_data_set_bool(obj, "focus", _checkFocus);
	_regex.Save(obj);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionProcess::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_process = obs_data_get_string(obj, "process");
	_checkFocus = obs_data_get_bool(obj, "focus");
	// Fall back to partial match regex as default for old version
	if (!obs_data_has_user_value(obj, "version")) {
		_regex.SetEnabled(true);
	} else {
		_regex.Load(obj);
	}
	return true;
}

std::string MacroConditionProcess::GetShortDesc() const
{
	return _process;
}

void MacroConditionProcess::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar("name",
		   obs_module_text("AdvSceneSwitcher.tempVar.process.name"));
}

MacroConditionProcessEdit::MacroConditionProcessEdit(
	QWidget *parent, std::shared_ptr<MacroConditionProcess> entryData)
	: QWidget(parent),
	  _processSelection(new QComboBox()),
	  _regex(new RegexConfigWidget(this)),
	  _focused(new QCheckBox()),
	  _focusProcess(new QLabel()),
	  _focusLayout(new QHBoxLayout())
{
	_processSelection->setEditable(true);
	_processSelection->setMaxVisibleItems(20);

	QWidget::connect(_processSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ProcessChanged(const QString &)));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));
	QWidget::connect(_focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));
	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdateFocusProcess()));

	PopulateProcessSelection(_processSelection);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{processes}}", _processSelection},
		{"{{regex}}", _regex},
		{"{{focused}}", _focused},
		{"{{focusProcess}}", _focusProcess},
	};

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.process.entry"),
		entryLayout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
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

	auto lock = LockContext();
	_entryData->_process = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionProcessEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionProcessEdit::FocusChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_checkFocus = state;
	SetWidgetVisibility();
}

void MacroConditionProcessEdit::UpdateFocusProcess()
{
	std::string name;
	GetForegroundProcessName(name);
	_focusProcess->setText(QString::fromStdString(name));
}

void MacroConditionProcessEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	SetLayoutVisible(_focusLayout, _entryData->_checkFocus);
	adjustSize();
}

void MacroConditionProcessEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_processSelection->setCurrentText(_entryData->_process.c_str());
	_regex->SetRegexConfig(_entryData->_regex);
	_focused->setChecked(_entryData->_checkFocus);
	SetWidgetVisibility();
}

} // namespace advss
