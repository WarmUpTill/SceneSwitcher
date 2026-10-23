#include "macro-action-variable-string.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "utility.hpp"

#include <QRegularExpression>

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{10,
	 {
		 {Action::PAD, "AdvSceneSwitcher.action.variable.type.padValue",
		  "AdvSceneSwitcher.action.variable.layout.pad"},
		 {Action::TRUNCATE,
		  "AdvSceneSwitcher.action.variable.type.truncateValue",
		  "AdvSceneSwitcher.action.variable.layout.truncate"},
		 {Action::SUBSTRING,
		  "AdvSceneSwitcher.action.variable.type.subString"},
		 {Action::FIND_AND_REPLACE,
		  "AdvSceneSwitcher.action.variable.type.findAndReplace"},
		 {Action::STRING_LENGTH,
		  "AdvSceneSwitcher.action.variable.type.stringLength",
		  "AdvSceneSwitcher.action.variable.layout.strValue", true},
		 {Action::TRIM, "AdvSceneSwitcher.action.variable.type.trim"},
		 {Action::CHANGE_CASE,
		  "AdvSceneSwitcher.action.variable.type.changeCase",
		  "AdvSceneSwitcher.action.variable.layout.changeCase"},
	 },
	 []() { return std::make_unique<VariableActionString>(); },
	 VariableActionStringEdit::Create});

static std::string truncate(const std::string &input,
			    VariableActionString::Direction direction,
			    size_t length)
{
	if (input.length() <= length) {
		return input;
	}

	if (direction == VariableActionString::Direction::RIGHT) {
		return input.substr(0, length);
	} else {
		return input.substr(input.length() - length);
	}
}

static std::string pad(const std::string &input,
		       VariableActionString::Direction direction, size_t length,
		       char paddingChar)
{
	if (input.length() >= length) {
		return input;
	}

	if (direction == VariableActionString::Direction::RIGHT) {
		return input +
		       std::string(length - input.length(), paddingChar);
	} else {
		return std::string(length - input.length(), paddingChar) +
		       input;
	}
}

std::unique_ptr<VariableActionGroup> VariableActionString::Copy() const
{
	return std::make_unique<VariableActionString>(*this);
}

void VariableActionString::Save(obs_data_t *obj,
				const MacroActionVariable &) const
{
	_subStringStart.Save(obj, "subStringStart");
	_subStringSize.Save(obj, "subStringSize");
	obs_data_set_string(obj, "regexPattern", _regexPattern.c_str());
	_regexMatchIdx.Save(obj, "regexMatchIdx");
	_findRegex.Save(obj, "findRegex");
	_findStr.Save(obj, "findStr");
	_replaceStr.Save(obj, "replaceStr");
	_subStringRegex.Save(obj);
	obs_data_set_int(obj, "direction", static_cast<int>(_direction));
	_stringLength.Save(obj, "stringLength");
	obs_data_set_int(obj, "paddingChar", _paddingChar);
	obs_data_set_int(obj, "caseType", static_cast<int>(_caseType));
}

void VariableActionString::Load(obs_data_t *obj, MacroActionVariable &)
{
	_subStringRegex.Load(obj);
	_regexPattern = obs_data_get_string(obj, "regexPattern");
	_findRegex.Load(obj, "findRegex");
	_findStr.Load(obj, "findStr");
	_replaceStr.Load(obj, "replaceStr");
	_direction = static_cast<Direction>(obs_data_get_int(obj, "direction"));
	_stringLength.Load(obj, "stringLength");
	if (obs_data_has_user_value(obj, "paddingChar")) {
		_paddingChar = obs_data_get_int(obj, "paddingChar");
	} else {
		_paddingChar = ' ';
	}
	_caseType = static_cast<CaseType>(obs_data_get_int(obj, "caseType"));

	// Convert old data format
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, "version")) {
		_subStringStart = obs_data_get_int(obj, "subStringStart") + 1;
		_subStringSize = obs_data_get_int(obj, "subStringSize");
		_regexMatchIdx = obs_data_get_int(obj, "regexMatchIdx") + 1;
	} else {
		_subStringStart.Load(obj, "subStringStart");
		_subStringSize.Load(obj, "subStringSize");
		_regexMatchIdx.Load(obj, "regexMatchIdx");
	}
}

void VariableActionString::ResolveVariables()
{
	_findStr.ResolveVariables();
	_replaceStr.ResolveVariables();
}

void VariableActionString::HandleIndexSubString(Variable &var)
{
	const auto curValue = var.Value();
	const auto subStringSize = _subStringSize.GetValue();
	const auto subStringStart = _subStringStart.GetValue() - 1;

	try {
		if (subStringSize == 0) {
			var.SetValue(curValue.substr(subStringStart));
			return;
		}

		var.SetValue(curValue.substr(subStringStart, subStringSize));
	} catch (const std::out_of_range &) {
		vblog(LOG_WARNING,
		      "invalid start index \"%d\" selected for substring of \"%s\" of variable \"%s\"",
		      subStringStart, curValue.c_str(), var.Name().c_str());
	}
}

void VariableActionString::HandleRegexSubString(Variable &var)
{
	const auto curValue = var.Value();
	auto regex = _subStringRegex.GetRegularExpression(_regexPattern);
	if (!regex.isValid()) {
		return;
	}

	auto it = regex.globalMatch(QString::fromStdString(curValue));
	const auto regexMatchIndex = _regexMatchIdx.GetValue() - 1;
	for (int idx = 0; idx < regexMatchIndex; idx++) {
		if (!it.hasNext()) {
			return;
		}
		it.next();
	}

	if (!it.hasNext()) {
		return;
	}

	auto match = it.next();
	var.SetValue(match.captured(0).toStdString());
}

void VariableActionString::HandleFindAndReplace(Variable &var)
{
	auto value = var.Value();
	if (!_findRegex.Enabled()) {
		ReplaceAll(value, _findStr, _replaceStr);
		var.SetValue(value);
		return;
	}

	QString replacement = QString::fromStdString(_replaceStr);
	auto regex = _findRegex.GetRegularExpression(_findStr);
	auto result = QString::fromStdString(value);
	result = result.replace(regex, replacement);
	var.SetValue(result.toStdString());
}

void VariableActionString::HandleCaseChange(Variable &var)
{
	auto value = QString::fromStdString(var.Value());

	switch (_caseType) {
	case CaseType::LOWER_CASE:
		var.SetValue(value.toLower().toStdString());
		break;
	case CaseType::UPPER_CASE:
		var.SetValue(value.toUpper().toStdString());
		break;
	case CaseType::CAPITALIZED: {
		if (value.isEmpty()) {
			return;
		}

		value[0] = value[0].toUpper();
		var.SetValue(value.toStdString());
		break;
	}
	case CaseType::START_CASE: {
		auto partList = value.split(QRegularExpression("\\b"));

		for (auto &part : partList) {
			if (part.isEmpty()) {
				continue;
			}

			part[0] = part[0].toUpper();
		}

		var.SetValue(partList.join("").toStdString());

		break;
	}
	}
}

bool VariableActionString::Perform(MacroActionVariable &action,
				   MacroActionVariable::Action type,
				   Variable &var)
{
	switch (type) {
	case Action::PAD:
		var.SetValue(pad(var.Value(), _direction, _stringLength,
				 _paddingChar));
		break;
	case Action::TRUNCATE:
		var.SetValue(truncate(var.Value(), _direction, _stringLength));
		break;
	case Action::SUBSTRING:
		if (_subStringRegex.Enabled()) {
			HandleRegexSubString(var);
			break;
		}
		HandleIndexSubString(var);
		break;
	case Action::FIND_AND_REPLACE:
		HandleFindAndReplace(var);
		break;
	case Action::STRING_LENGTH:
		var.SetValue(std::string(action._strValue).length());
		break;
	case Action::TRIM:
		var.SetValue(QString::fromStdString(var.Value())
				     .trimmed()
				     .toStdString());
		break;
	case Action::CHANGE_CASE:
		HandleCaseChange(var);
		break;
	default:
		break;
	}
	return true;
}

static void populateCaseTypeSelection(QComboBox *list)
{
	const static std::map<VariableActionString::CaseType, std::string>
		caseTypes = {
			{VariableActionString::CaseType::LOWER_CASE,
			 "AdvSceneSwitcher.action.variable.case.type.lowerCase"},
			{VariableActionString::CaseType::UPPER_CASE,
			 "AdvSceneSwitcher.action.variable.case.type.upperCase"},
			{VariableActionString::CaseType::CAPITALIZED,
			 "AdvSceneSwitcher.action.variable.case.type.capitalized"},
			{VariableActionString::CaseType::START_CASE,
			 "AdvSceneSwitcher.action.variable.case.type.startCase"},
		};

	for (const auto &[type, name] : caseTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(type));
	}
}

static void populateDirectionSelection(QComboBox *list)
{
	list->addItems(
		QStringList()
		<< obs_module_text(
			   "AdvSceneSwitcher.action.variable.truncateOrPadDirection.left")
		<< obs_module_text(
			   "AdvSceneSwitcher.action.variable.truncateOrPadDirection.right"));
}

VariableActionStringEdit::VariableActionStringEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionString>()
			  : nullptr),
	  _substringLayout(new QVBoxLayout()),
	  _subStringControlsLayout(new QHBoxLayout()),
	  _subStringStart(new VariableSpinBox(this)),
	  _subStringSize(new VariableSpinBox(this)),
	  _subStringRegex(new RegexConfigWidget(this)),
	  _regexPattern(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _regexMatchIdx(new VariableSpinBox(this)),
	  _findReplaceLayout(new QHBoxLayout()),
	  _findRegex(new RegexConfigWidget(this)),
	  _findStr(new VariableTextEdit(this, 10, 1, 1)),
	  _replaceStr(new VariableTextEdit(this, 10, 1, 1)),
	  _direction(new QComboBox(this)),
	  _stringLength(new VariableSpinBox(this)),
	  _paddingCharSelection(new SingleCharSelection(this)),
	  _caseType(new FilterComboBox(this))
{
	_subStringStart->setMinimum(1);
	_subStringStart->setMaximum(99999);
	_subStringStart->setSpecialValueText(obs_module_text(
		"AdvSceneSwitcher.action.variable.subString.begin"));
	_subStringSize->setMinimum(0);
	_subStringSize->setMaximum(99999);
	_subStringSize->setSpecialValueText(obs_module_text(
		"AdvSceneSwitcher.action.variable.subString.all"));
	_regexMatchIdx->setMinimum(1);
	_regexMatchIdx->setMaximum(999);
	_regexMatchIdx->setSuffix(".");
	_replaceStr->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.variable.findAndReplace.replace.tooltip"));
	_stringLength->setMaximum(999);
	populateDirectionSelection(_direction);
	populateCaseTypeSelection(_caseType);

	QWidget::connect(
		_subStringStart,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SubStringStartChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_subStringSize,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SubStringSizeChanged(const NumberVariable<int> &)));
	QWidget::connect(_subStringRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(SubStringRegexChanged(const RegexConfig &)));
	QWidget::connect(_regexPattern, SIGNAL(textChanged()), this,
			 SLOT(RegexPatternChanged()));
	QWidget::connect(
		_regexMatchIdx,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(RegexMatchIdxChanged(const NumberVariable<int> &)));
	QWidget::connect(_findRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(FindRegexChanged(const RegexConfig &)));
	QWidget::connect(_findStr, SIGNAL(textChanged()), this,
			 SLOT(FindStrValueChanged()));
	QWidget::connect(_replaceStr, SIGNAL(textChanged()), this,
			 SLOT(ReplaceStrValueChanged()));
	QWidget::connect(_direction, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(DirectionChanged(int)));
	QWidget::connect(
		_stringLength,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(StringLengthChanged(const NumberVariable<int> &)));
	QWidget::connect(_paddingCharSelection,
			 SIGNAL(CharChanged(const QString &)), this,
			 SLOT(CharSelectionChanged(const QString &)));
	QWidget::connect(_caseType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CaseTypeChanged(int)));

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.findAndReplace"),
		_findReplaceLayout,
		{{"{{findStr}}", _findStr},
		 {"{{findRegex}}", _findRegex},
		 {"{{replaceStr}}", _replaceStr}},
		false);

	auto regexConfigLayout = new QHBoxLayout;
	regexConfigLayout->addWidget(_subStringRegex);
	regexConfigLayout->addStretch();

	_substringLayout->addLayout(_subStringControlsLayout);
	_substringLayout->addWidget(_regexPattern);
	_substringLayout->addLayout(regexConfigLayout);

	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(_substringLayout);
	layout->addLayout(_findReplaceLayout);
	setLayout(layout);
}

VariableActionGroupEdit *
VariableActionStringEdit::Create(QWidget *parent,
				 std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionStringEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionStringEdit::GetInlineWidgets()
{
	return {
		{"{{direction}}", _direction},
		{"{{stringLength}}", _stringLength},
		{"{{paddingCharSelection}}", _paddingCharSelection},
		{"{{caseType}}", _caseType},
	};
}

void VariableActionStringEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_subStringStart->SetValue(_data->_subStringStart);
	_subStringSize->SetValue(_data->_subStringSize);
	_subStringRegex->SetRegexConfig(_data->_subStringRegex);
	_findRegex->SetRegexConfig(_data->_findRegex);
	_regexPattern->setPlainText(
		QString::fromStdString(_data->_regexPattern));
	_regexMatchIdx->SetValue(_data->_regexMatchIdx);
	_findStr->setPlainText(_data->_findStr);
	_replaceStr->setPlainText(_data->_replaceStr);
	_direction->setCurrentIndex(static_cast<int>(_data->_direction));
	_stringLength->SetValue(_data->_stringLength);
	_paddingCharSelection->setText(QChar::fromLatin1(_data->_paddingChar));
	_caseType->setCurrentIndex(
		_caseType->findData(static_cast<int>(_data->_caseType)));
	_loading = false;
}

void VariableActionStringEdit::SetWidgetVisibility(
	MacroActionVariable::Action action)
{
	if (!_data) {
		return;
	}

	const std::unordered_map<std::string, QWidget *> substringWidgets = {
		{"{{subStringRegex}}", _subStringRegex},
		{"{{subStringStart}}", _subStringStart},
		{"{{subStringSize}}", _subStringSize},
		{"{{regexMatchIdx}}", _regexMatchIdx},
	};
	for (const auto &[_, widget] : substringWidgets) {
		_subStringControlsLayout->removeWidget(widget);
	}
	ClearLayout(_subStringControlsLayout);

	const bool showRegex = _data->_subStringRegex.Enabled();
	PlaceWidgets(
		obs_module_text(
			showRegex
				? "AdvSceneSwitcher.action.variable.layout.substringRegex"
				: "AdvSceneSwitcher.action.variable.layout.substringIndex"),
		_subStringControlsLayout, substringWidgets);
	_subStringStart->setVisible(!showRegex);
	_subStringSize->setVisible(!showRegex);
	_regexMatchIdx->setVisible(showRegex);

	const bool isSubstring = action == Action::SUBSTRING;
	const bool isFindAndReplace = action == Action::FIND_AND_REPLACE;
	SetLayoutVisible(_substringLayout, isSubstring);
	_regexPattern->setVisible(showRegex && isSubstring);
	SetLayoutVisible(_findReplaceLayout, isFindAndReplace);
	setVisible(isSubstring || isFindAndReplace);

	UpdateSize();
}

void VariableActionStringEdit::SubStringStartChanged(
	const NumberVariable<int> &start)
{
	GUARD_LOADING_AND_LOCK();
	_data->_subStringStart = start;
}

void VariableActionStringEdit::SubStringSizeChanged(
	const NumberVariable<int> &size)
{
	GUARD_LOADING_AND_LOCK();
	_data->_subStringSize = size;
}

void VariableActionStringEdit::SubStringRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_data->_subStringRegex = conf;
	SetWidgetVisibility(_entryData->_action);
}

void VariableActionStringEdit::RegexPatternChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_regexPattern = _regexPattern->toPlainText().toStdString();
	UpdateSize();
}

void VariableActionStringEdit::RegexMatchIdxChanged(
	const NumberVariable<int> &index)
{
	GUARD_LOADING_AND_LOCK();
	_data->_regexMatchIdx = index;
}

void VariableActionStringEdit::FindStrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_findStr = _findStr->toPlainText().toStdString();
	UpdateSize();
}

void VariableActionStringEdit::FindRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_data->_findRegex = conf;
	UpdateSize();
}

void VariableActionStringEdit::ReplaceStrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_replaceStr = _replaceStr->toPlainText().toStdString();
	UpdateSize();
}

void VariableActionStringEdit::DirectionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_direction = static_cast<VariableActionString::Direction>(value);
}

void VariableActionStringEdit::StringLengthChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_stringLength = value;
}

void VariableActionStringEdit::CharSelectionChanged(const QString &character)
{
	GUARD_LOADING_AND_LOCK();
	if (character.isEmpty()) {
		_data->_paddingChar = ' ';
	} else {
		_data->_paddingChar = character.toStdString().at(0);
	}
}

void VariableActionStringEdit::CaseTypeChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_data->_caseType = static_cast<VariableActionString::CaseType>(
		_caseType->itemData(index).toInt());
}

} // namespace advss
