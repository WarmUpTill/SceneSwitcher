#include "macro-condition-process.hpp"
#include "layout-helpers.hpp"
#include "platform-funcs.hpp"
#include "selection-helpers.hpp"

#include <regex>

namespace advss {

const std::string MacroConditionProcess::id = "process";

bool MacroConditionProcess::_registered = MacroConditionFactory::Register(
	MacroConditionProcess::id,
	{MacroConditionProcess::Create, MacroConditionProcessEdit::Create,
	 "AdvSceneSwitcher.condition.process"});

bool MacroConditionProcess::CheckCondition()
{
	const auto foregroundProcessName = GetForegroundProcessName();
	SetVariableValue(foregroundProcessName);

	const QString proc = QString::fromStdString(_process);

	if (_checkFocus) {
		// Check name and path against the same foreground process
		// instance to avoid false positives when multiple processes
		// share the same name
		const auto foregroundPath = GetForegroundProcessPath();
		const QString foregroundName =
			QString::fromStdString(foregroundProcessName);

		SetTempVarValue("name", foregroundProcessName);
		SetTempVarValue("path", foregroundPath);

		bool nameMatches =
			_regex.Enabled() ? _regex.Matches(foregroundName, proc)
					 : (foregroundName == proc);
		if (!nameMatches) {
			return false;
		}

		if (_checkPath) {
			const QString pathPattern =
				QString::fromStdString(_processPath);
			const QString qForegroundPath =
				QString::fromStdString(foregroundPath);
			bool pathMatches =
				_pathRegex.Enabled()
					? _pathRegex.Matches(qForegroundPath,
							     pathPattern)
					: qForegroundPath == pathPattern;
			if (!pathMatches) {
				return false;
			}
		}

		return true;
	}

	const auto runningProcesses = GetProcessList();

	for (const auto &process : runningProcesses) {
		bool nameMatches = _regex.Enabled()
					   ? _regex.Matches(process, proc)
					   : (process == proc);
		if (!nameMatches) {
			continue;
		}

		if (!_checkPath) {
			SetTempVarValue("name", process.toStdString());
			return true;
		}

		const auto paths = GetProcessPathsFromName(process);

		const QString pathPattern =
			QString::fromStdString(_processPath);
		bool foundMatchingPath = false;
		for (const auto &path : paths) {
			bool pathMatches =
				_pathRegex.Enabled()
					? _pathRegex.Matches(path, pathPattern)
					: path == pathPattern;
			if (pathMatches) {
				SetTempVarValue("path", path.toStdString());
				foundMatchingPath = true;
				break;
			}
		}

		if (!foundMatchingPath) {
			continue;
		}

		SetTempVarValue("name", process.toStdString());
		return true;
	}

	return false;
}

bool MacroConditionProcess::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_process.Save(obj, "process");
	_regex.Save(obj);
	obs_data_set_bool(obj, "focus", _checkFocus);
	obs_data_set_bool(obj, "checkPath", _checkPath);
	_processPath.Save(obj, "processPath");
	_pathRegex.Save(obj, "pathRegex");
	obs_data_set_int(obj, "version", 2);
	return true;
}

bool MacroConditionProcess::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_process.Load(obj, "process");
	_checkFocus = obs_data_get_bool(obj, "focus");
	// Fall back to partial match regex as default for old version
	if (!obs_data_has_user_value(obj, "version")) {
		_regex.SetEnabled(true);
	} else {
		_regex.Load(obj);
	}
	_checkPath = obs_data_get_bool(obj, "checkPath");
	_processPath.Load(obj, "processPath");
	_pathRegex.Load(obj, "pathRegex");
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
	AddTempvar("path",
		   obs_module_text("AdvSceneSwitcher.tempVar.process.path"));
}

MacroConditionProcessEdit::MacroConditionProcessEdit(
	QWidget *parent, std::shared_ptr<MacroConditionProcess> entryData)
	: QWidget(parent),
	  _processSelection(new QComboBox()),
	  _regex(new RegexConfigWidget(this)),
	  _focused(new QCheckBox()),
	  _focusProcess(new QLabel()),
	  _focusLayout(new QHBoxLayout()),
	  _checkPath(new QCheckBox()),
	  _processPath(new VariableLineEdit(this)),
	  _pathRegex(new RegexConfigWidget(this)),
	  _pathLayout(new QHBoxLayout())
{
	_processSelection->setEditable(true);
	_processSelection->setMaxVisibleItems(20);
	_processSelection->setToolTip(
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));

	_processPath->setToolTip(
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));

	QWidget::connect(_processSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ProcessChanged(const QString &)));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_focused, SIGNAL(stateChanged(int)), this,
			 SLOT(FocusChanged(int)));
	QWidget::connect(_checkPath, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckPathChanged(int)));
	QWidget::connect(_processPath, SIGNAL(textChanged(const QString &)),
			 this, SLOT(ProcessPathChanged(const QString &)));
	QWidget::connect(_pathRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(PathRegexChanged(const RegexConfig &)));
	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdateFocusProcess()));

	PopulateProcessSelection(_processSelection);

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{processes}}", _processSelection},
		{"{{regex}}", _regex},
		{"{{focused}}", _focused},
		{"{{focusProcess}}", _focusProcess},
		{"{{checkPath}}", _checkPath},
		{"{{path}}", _processPath},
		{"{{pathRegex}}", _pathRegex},
	};

	auto entryLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.process.layout"),
		entryLayout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.process.layout.focus"),
		     _focusLayout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.process.layout.path"),
		     _pathLayout, widgetPlaceholders);
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_focusLayout);
	mainLayout->addLayout(_pathLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	_timer.start(1000);
}

void MacroConditionProcessEdit::ProcessChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_process = text.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionProcessEdit::showEvent(QShowEvent *)
{
	const QSignalBlocker b(_processSelection);
	const auto text = _processSelection->currentText();
	_processSelection->clear();
	PopulateProcessSelection(_processSelection);
	_processSelection->setCurrentText(text);
}

void MacroConditionProcessEdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionProcessEdit::FocusChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_checkFocus = state;
	SetWidgetVisibility();
}

void MacroConditionProcessEdit::CheckPathChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_checkPath = state;
	SetWidgetVisibility();
}

void MacroConditionProcessEdit::ProcessPathChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_processPath = text.toStdString();
}

void MacroConditionProcessEdit::PathRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_pathRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroConditionProcessEdit::UpdateFocusProcess()
{
	_focusProcess->setText(
		QString::fromStdString(GetForegroundProcessName()));
}

void MacroConditionProcessEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	SetLayoutVisible(_focusLayout, _entryData->_checkFocus);
	_processPath->setVisible(_entryData->_checkPath);
	_pathRegex->setVisible(_entryData->_checkPath);
	if (_entryData->_checkPath) {
		RemoveStretchIfPresent(_pathLayout);
	} else {
		AddStretchIfNecessary(_pathLayout);
	}
	adjustSize();
	updateGeometry();
}

void MacroConditionProcessEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_processSelection->setCurrentText(
		_entryData->_process.UnresolvedValue().c_str());
	_regex->SetRegexConfig(_entryData->_regex);
	_focused->setChecked(_entryData->_checkFocus);
	_checkPath->setChecked(_entryData->_checkPath);
	_processPath->setText(_entryData->_processPath);
	_pathRegex->SetRegexConfig(_entryData->_pathRegex);
	SetWidgetVisibility();
}

} // namespace advss
