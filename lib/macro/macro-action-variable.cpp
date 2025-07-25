#include "macro-action-variable.hpp"
#include "advanced-scene-switcher.hpp"
#include "json-helpers.hpp"
#include "layout-helpers.hpp"
#include "math-helpers.hpp"
#include "macro-condition-edit.hpp"
#include "macro.hpp"
#include "non-modal-dialog.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <random>

namespace advss {

const std::string MacroActionVariable::id = "variable";

bool MacroActionVariable::_registered = MacroActionFactory::Register(
	MacroActionVariable::id,
	{MacroActionVariable::Create, MacroActionVariableEdit::Create,
	 "AdvSceneSwitcher.action.variable"});

static void apppend(Variable &var, const std::string &value)
{
	auto currentValue = var.Value();
	var.SetValue(currentValue + value);
}

static void modifyNumValue(Variable &var, double val, const bool increment)
{
	auto current = var.DoubleValue();
	if (!current.has_value()) {
		return;
	}

	if (increment) {
		var.SetValue(*current + val);
	} else {
		var.SetValue(*current - val);
	}
}

MacroActionVariable::~MacroActionVariable()
{
	DecrementCurrentSegmentVariableRef();
}

void MacroActionVariable::HandleIndexSubString(Variable *var)
{
	const auto curValue = var->Value();
	const auto subStringSize = _subStringSize.GetValue();
	const auto subStringStart = _subStringStart.GetValue() - 1;

	try {
		if (subStringSize == 0) {
			var->SetValue(curValue.substr(subStringStart));
			return;
		}

		var->SetValue(curValue.substr(subStringStart, subStringSize));
	} catch (const std::out_of_range &) {
		vblog(LOG_WARNING,
		      "invalid start index \"%d\" selected for substring of \"%s\" of variable \"%s\"",
		      subStringStart, curValue.c_str(), var->Name().c_str());
	}
}

void MacroActionVariable::HandleRegexSubString(Variable *var)
{
	const auto curValue = var->Value();
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
	var->SetValue(match.captured(0).toStdString());
}

void MacroActionVariable::HandleFindAndReplace(Variable *var)
{
	auto value = var->Value();
	if (!_findRegex.Enabled()) {
		ReplaceAll(value, _findStr, _replaceStr);
		var->SetValue(value);
		return;
	}

	QString replacement = QString::fromStdString(_replaceStr);
	auto regex = _findRegex.GetRegularExpression(_findStr);
	auto result = QString::fromStdString(value);
	result = result.replace(regex, replacement);
	var->SetValue(result.toStdString());
}

void MacroActionVariable::HandleMathExpression(Variable *var)
{
	auto result = EvalMathExpression(_mathExpression);
	if (std::holds_alternative<std::string>(result)) {
		blog(LOG_WARNING, "%s", std::get<std::string>(result).c_str());
		return;
	}
	var->SetValue(std::get<double>(result));
}

void MacroActionVariable::HandleCaseChange(Variable *var)
{
	auto value = QString::fromStdString(var->Value());

	switch (_caseType) {
	case CaseType::LOWER_CASE:
		var->SetValue(value.toLower().toStdString());
		break;
	case CaseType::UPPER_CASE:
		var->SetValue(value.toUpper().toStdString());
		break;
	case CaseType::CAPITALIZED: {
		if (value.isEmpty()) {
			return;
		}

		value[0] = value[0].toUpper();
		var->SetValue(value.toStdString());
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

		var->SetValue(partList.join("").toStdString());

		break;
	}
	}
}
struct GetSceneItemNameHelper {
	int curIdx = 0;
	int targetIdx = 0;
	std::string name = "";
};

static bool getSceneItemAtIdx(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto data = reinterpret_cast<GetSceneItemNameHelper *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItemAtIdx, ptr);
	}
	if (data->curIdx == data->targetIdx) {
		data->name =
			obs_source_get_name(obs_sceneitem_get_source(item));
		data->curIdx--;
		return false;
	}
	data->curIdx--;
	return true;
}

void MacroActionVariable::SetToSceneItemName(Variable *var)
{
	auto weakSource = _scene.GetScene();
	if (!weakSource) {
		var->SetValue("");
		return;
	}
	auto index = _sceneItemIndex.GetValue();
	if (index < 1) {
		var->SetValue("");
		return;
	}
	auto sceneItemCount = GetSceneItemCount(weakSource);
	OBSSourceAutoRelease source = obs_weak_source_get_source(weakSource);
	auto scene = obs_scene_from_source(source);
	GetSceneItemNameHelper data{sceneItemCount, index};
	obs_scene_enum_items(scene, getSceneItemAtIdx, &data);
	var->SetValue(data.name);
}

void MacroActionVariable::GenerateRandomNumber(Variable *var)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());

	double start = _randomNumberStart;
	double end = _randomNumberEnd;

	if (start > end) {
		std::swap(start, end);
	}

	if (_generateInteger) {
		std::uniform_int_distribution<int> dis(start, end);
		var->SetValue(dis(gen));
	} else {
		std::uniform_real_distribution<double> dis(start, end);
		var->SetValue(dis(gen));
	}
}

struct AskForInputParams {
	AskForInputParams(const QString &prompt_, const QString &placeholder_)
		: prompt(prompt_),
		  placeholder(placeholder_){};
	QString prompt;
	QString placeholder;
	std::optional<std::string> result;
	std::atomic_bool resultReady = {false};
};

static void askForInput(void *param)
{
	auto parameters =
		*static_cast<std::shared_ptr<AskForInputParams> *>(param);
	auto dialog = new NonModalMessageDialog(
		parameters->prompt, NonModalMessageDialog::Type::INPUT, false);
	dialog->SetInput(parameters->placeholder);
	parameters->result = dialog->GetInput();
	parameters->resultReady = true;
}

static std::string truncate(const std::string &input,
			    MacroActionVariable::Direction direction,
			    size_t length)
{
	if (input.length() <= length) {
		return input;
	}

	if (direction == MacroActionVariable::Direction::RIGHT) {
		return input.substr(0, length);
	} else {
		return input.substr(input.length() - length);
	}
}

static std::string pad(const std::string &input,
		       MacroActionVariable::Direction direction, size_t length,
		       char paddingChar)
{
	if (input.length() >= length) {
		return input;
	}

	if (direction == MacroActionVariable::Direction::RIGHT) {
		return input +
		       std::string(length - input.length(), paddingChar);
	} else {
		return std::string(length - input.length(), paddingChar) +
		       input;
	}
}

bool MacroActionVariable::PerformAction()
{
	auto var = _variable.lock();
	if (!var) {
		return true;
	}

	switch (_action) {
	case Action::SET_VALUE:
		var->SetValue(_strValue);
		break;
	case Action::APPEND:
		apppend(*var, _strValue);
		break;
	case Action::APPEND_VAR: {
		auto var2 = _variable2.lock();
		if (!var2) {
			return true;
		}
		apppend(*var, var2->Value());
		break;
	}
	case Action::INCREMENT:
		modifyNumValue(*var, _numValue.GetValue(), true);
		break;
	case Action::DECREMENT:
		modifyNumValue(*var, _numValue.GetValue(), false);
		break;
	case Action::SET_CONDITION_VALUE:
	case Action::SET_ACTION_VALUE: {
		auto m = GetMacro();
		if (!m) {
			return true;
		}
		if (GetSegmentIndexValue() == -1) {
			return true;
		}
		auto segment = _macroSegment.lock();
		if (!segment) {
			return true;
		}
		var->SetValue(segment->GetVariableValue());
		break;
	}
	case Action::ROUND_TO_INT: {
		auto curValue = var->DoubleValue();
		if (!curValue.has_value()) {
			return true;
		}
		var->SetValue(std::to_string(int(std::round(*curValue))));
		return true;
	}
	case Action::SUBSTRING: {
		if (_subStringRegex.Enabled()) {
			HandleRegexSubString(var.get());
			return true;
		}
		HandleIndexSubString(var.get());
		return true;
	}
	case Action::FIND_AND_REPLACE: {
		HandleFindAndReplace(var.get());
		return true;
	}
	case Action::MATH_EXPRESSION: {
		HandleMathExpression(var.get());
		return true;
	}
	case Action::USER_INPUT: {
		auto params = std::make_shared<AskForInputParams>(
			_useCustomPrompt
				? QString::fromStdString(_inputPrompt)
				: QString(obs_module_text(
						  "AdvSceneSwitcher.action.variable.askForValuePromptDefault"))
					  .arg(QString::fromStdString(
						  var->Name())),
			_useCustomPrompt && _useInputPlaceholder
				? QString::fromStdString(_inputPlaceholder)
				: "");
		obs_queue_task(OBS_TASK_UI, askForInput, &params, false);
		while (!params->resultReady) {
			if (GetMacro()->GetStop()) {
				return false;
			}
			std::this_thread::sleep_for(
				std::chrono::milliseconds(100));
		}
		if (!params->result.has_value()) {
			return true;
		}
		var->SetValue(*params->result);
		return true;
	}
	case Action::ENV_VARIABLE: {
		auto value = std::getenv(_envVariableName.c_str());
		if (value) {
			var->SetValue(value);
		}
		return true;
	}
	case Action::SCENE_ITEM_COUNT: {
		var->SetValue(GetSceneItemCount(_scene.GetScene(false)));
		return true;
	}
	case Action::STRING_LENGTH: {
		var->SetValue(std::string(_strValue).length());
		return true;
	}
	case Action::EXTRACT_JSON: {
		auto value = GetJsonField(var->Value(), _strValue);
		if (!value.has_value()) {
			return true;
		}
		var->SetValue(*value);
		return true;
	}
	case Action::QUERY_JSON: {
		auto value = QueryJson(var->Value(), _jsonQuery);
		if (!value.has_value()) {
			return true;
		}
		var->SetValue(*value);
		return true;
	}
	case Action::ARRAY_JSON: {
		auto value = AccessJsonArrayIndex(var->Value(), _jsonIndex);
		if (!value.has_value()) {
			return true;
		}
		var->SetValue(*value);
		return true;
	}
	case Action::SET_TO_TEMPVAR: {
		auto tempVar = _tempVar.GetTempVariable(GetMacro());
		if (!tempVar) {
			return true;
		}
		auto value = tempVar->Value();
		if (!value) {
			return true;
		}
		var->SetValue(*value);
		return true;
	}
	case Action::SCENE_ITEM_NAME:
		SetToSceneItemName(var.get());
		return true;
	case Action::PAD:
		var->SetValue(pad(var->Value(), _direction, _stringLength,
				  _paddingChar));
		return true;
	case Action::TRUNCATE:
		var->SetValue(
			truncate(var->Value(), _direction, _stringLength));
		return true;
	case Action::SWAP_VALUES: {
		auto var2 = _variable2.lock();
		if (!var2) {
			return true;
		}

		auto tempValue = var->Value();
		var->SetValue(var2->Value());
		var2->SetValue(tempValue);
		return true;
	}
	case Action::TRIM: {
		var->SetValue(QString::fromStdString(var->Value())
				      .trimmed()
				      .toStdString());
		return true;
	}
	case Action::CHANGE_CASE:
		HandleCaseChange(var.get());
		return true;
	case Action::RANDOM_NUMBER:
		GenerateRandomNumber(var.get());
		return true;
	}

	return true;
}

bool MacroActionVariable::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "variableName",
			    GetWeakVariableName(_variable).c_str());
	obs_data_set_string(obj, "variable2Name",
			    GetWeakVariableName(_variable2).c_str());
	_strValue.Save(obj, "strValue");
	_numValue.Save(obj, "numValue");
	obs_data_set_int(obj, "condition", static_cast<int>(_action));
	obs_data_set_int(obj, "segmentIdx", GetSegmentIndexValue());
	_subStringStart.Save(obj, "subStringStart");
	_subStringSize.Save(obj, "subStringSize");
	obs_data_set_string(obj, "regexPattern", _regexPattern.c_str());
	_regexMatchIdx.Save(obj, "regexMatchIdx");
	_findRegex.Save(obj, "findRegex");
	_findStr.Save(obj, "findStr");
	_replaceStr.Save(obj, "replaceStr");
	_subStringRegex.Save(obj);
	_mathExpression.Save(obj, "mathExpression");
	obs_data_set_bool(obj, "useCustomPrompt", _useCustomPrompt);
	_inputPrompt.Save(obj, "inputPrompt");
	obs_data_set_bool(obj, "useInputPlaceholder", _useInputPlaceholder);
	_inputPlaceholder.Save(obj, "inputPlaceholder");
	_envVariableName.Save(obj, "environmentVariableName");
	_scene.Save(obj);
	_tempVar.Save(obj);
	_sceneItemIndex.Save(obj, "sceneItemIndex");
	obs_data_set_int(obj, "direction", static_cast<int>(_direction));
	_stringLength.Save(obj, "stringLength");
	obs_data_set_int(obj, "paddingChar", _paddingChar);
	obs_data_set_int(obj, "caseType", static_cast<int>(_caseType));
	_randomNumberStart.Save(obj, "randomNumberStart");
	_randomNumberEnd.Save(obj, "randomNumberEnd");
	obs_data_set_bool(obj, "generateInteger", _generateInteger);
	_jsonQuery.Save(obj, "jsonQuery");
	_jsonIndex.Save(obj, "jsonIndex");

	obs_data_set_int(obj, "version", 1);

	return true;
}

bool MacroActionVariable::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_variable =
		GetWeakVariableByName(obs_data_get_string(obj, "variableName"));
	_variable2 = GetWeakVariableByName(
		obs_data_get_string(obj, "variable2Name"));
	_strValue.Load(obj, "strValue");
	_action = static_cast<Action>(obs_data_get_int(obj, "condition"));
	_segmentIdxLoadValue = obs_data_get_int(obj, "segmentIdx");
	_subStringRegex.Load(obj);
	_regexPattern = obs_data_get_string(obj, "regexPattern");
	_findRegex.Load(obj, "findRegex");
	_findStr.Load(obj, "findStr");
	_replaceStr.Load(obj, "replaceStr");
	_mathExpression.Load(obj, "mathExpression");
	_useCustomPrompt = obs_data_get_bool(obj, "useCustomPrompt");
	_inputPrompt.Load(obj, "inputPrompt");
	_useInputPlaceholder = obs_data_get_bool(obj, "useInputPlaceholder");
	_inputPlaceholder.Load(obj, "inputPlaceholder");
	_envVariableName.Load(obj, "environmentVariableName");
	_scene.Load(obj);
	_tempVar.Load(obj, GetMacro());
	_sceneItemIndex.Load(obj, "sceneItemIndex");
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
		_numValue = obs_data_get_double(obj, "numValue");
		_subStringStart = obs_data_get_int(obj, "subStringStart") + 1;
		_subStringSize = obs_data_get_int(obj, "subStringSize");
		_regexMatchIdx = obs_data_get_int(obj, "regexMatchIdx") + 1;
	} else {
		_numValue.Load(obj, "numValue");
		_subStringStart.Load(obj, "subStringStart");
		_subStringSize.Load(obj, "subStringSize");
		_regexMatchIdx.Load(obj, "regexMatchIdx");
	}

	_randomNumberStart.Load(obj, "randomNumberStart");
	_randomNumberEnd.Load(obj, "randomNumberEnd");
	_generateInteger = obs_data_get_bool(obj, "generateInteger");
	_jsonQuery.Load(obj, "jsonQuery");
	_jsonIndex.Load(obj, "jsonIndex");

	return true;
}

bool MacroActionVariable::PostLoad()
{
	SetSegmentIndexValue(_segmentIdxLoadValue);
	return true;
}

std::string MacroActionVariable::GetShortDesc() const
{
	return GetWeakVariableName(_variable);
}

std::shared_ptr<MacroAction> MacroActionVariable::Create(Macro *m)
{
	return std::make_shared<MacroActionVariable>(m);
}

std::shared_ptr<MacroAction> MacroActionVariable::Copy() const
{
	return std::make_shared<MacroActionVariable>(*this);
}

void MacroActionVariable::SetSegmentIndexValue(int value)
{
	DecrementCurrentSegmentVariableRef();

	auto m = GetMacro();
	if (!m) {
		_macroSegment.reset();
		return;
	}

	if (value < 0) {
		_macroSegment.reset();
		return;
	}

	std::shared_ptr<MacroSegment> segment;
	if (_action == Action::SET_CONDITION_VALUE) {
		if (value < (int)m->Conditions().size()) {
			segment = m->Conditions().at(value);
		}
	} else if (_action == Action::SET_ACTION_VALUE) {
		if (value < (int)m->Actions().size()) {
			segment = m->Actions().at(value);
		}
	}

	_macroSegment = segment;
	if (segment) {
		IncrementVariableRef(segment.get());
	}
}

int MacroActionVariable::GetSegmentIndexValue() const
{
	auto m = GetMacro();
	if (!m) {
		return -1;
	}

	auto segment = _macroSegment.lock();
	if (!segment) {
		return -1;
	}

	if (_action == Action::SET_CONDITION_VALUE) {
		auto it = std::find(m->Conditions().begin(),
				    m->Conditions().end(), segment);
		if (it != m->Conditions().end()) {
			return std::distance(m->Conditions().begin(), it);
		}
		return -1;
	} else if (_action == Action::SET_ACTION_VALUE) {
		auto it = std::find(m->Actions().begin(), m->Actions().end(),
				    segment);
		if (it != m->Actions().end()) {
			return std::distance(m->Actions().begin(), it);
		}
		return -1;
	}

	return -1;
}

void MacroActionVariable::ResolveVariablesToFixedValues()
{
	_strValue.ResolveVariables();
	_findStr.ResolveVariables();
	_replaceStr.ResolveVariables();
	_mathExpression.ResolveVariables();
	_inputPrompt.ResolveVariables();
	_inputPlaceholder.ResolveVariables();
	_envVariableName.ResolveVariables();
	_scene.ResolveVariables();
	_sceneItemIndex.ResolveVariables();
	_jsonQuery.ResolveVariables();
	_jsonIndex.ResolveVariables();
}

void MacroActionVariable::DecrementCurrentSegmentVariableRef()
{
	auto segment = _macroSegment.lock();
	if (!segment) {
		return;
	}

	DecrementVariableRef(segment.get());
}

static inline void populateActionSelection(QComboBox *list)
{
	static const std::vector<std::pair<
		std::variant<MacroActionVariable::Action, bool>, std::string>>
		actions = {
			{MacroActionVariable::Action::SET_VALUE,
			 "AdvSceneSwitcher.action.variable.type.set"},
			{MacroActionVariable::Action::APPEND,
			 "AdvSceneSwitcher.action.variable.type.append"},
			{MacroActionVariable::Action::PAD,
			 "AdvSceneSwitcher.action.variable.type.padValue"},
			{MacroActionVariable::Action::TRUNCATE,
			 "AdvSceneSwitcher.action.variable.type.truncateValue"},
			{MacroActionVariable::Action::SUBSTRING,
			 "AdvSceneSwitcher.action.variable.type.subString"},
			{MacroActionVariable::Action::FIND_AND_REPLACE,
			 "AdvSceneSwitcher.action.variable.type.findAndReplace"},
			{MacroActionVariable::Action::STRING_LENGTH,
			 "AdvSceneSwitcher.action.variable.type.stringLength"},
			{MacroActionVariable::Action::TRIM,
			 "AdvSceneSwitcher.action.variable.type.trim"},
			{MacroActionVariable::Action::CHANGE_CASE,
			 "AdvSceneSwitcher.action.variable.type.changeCase"},
			{true, ""}, // Separator

			{MacroActionVariable::Action::EXTRACT_JSON,
			 "AdvSceneSwitcher.action.variable.type.extractJsonField"},
			{MacroActionVariable::Action::QUERY_JSON,
			 "AdvSceneSwitcher.action.variable.type.queryJson"},
			{MacroActionVariable::Action::ARRAY_JSON,
			 "AdvSceneSwitcher.action.variable.type.accessJsonArray"},
			{true, ""}, // Separator

			{MacroActionVariable::Action::SET_TO_TEMPVAR,
			 "AdvSceneSwitcher.action.variable.type.setToTempvar"},
			{MacroActionVariable::Action::SET_CONDITION_VALUE,
			 "AdvSceneSwitcher.action.variable.type.setConditionValue"},
			{MacroActionVariable::Action::SET_ACTION_VALUE,
			 "AdvSceneSwitcher.action.variable.type.setActionValue"},
			{true, ""}, // Separator

			{MacroActionVariable::Action::MATH_EXPRESSION,
			 "AdvSceneSwitcher.action.variable.type.mathExpression"},
			{MacroActionVariable::Action::INCREMENT,
			 "AdvSceneSwitcher.action.variable.type.increment"},
			{MacroActionVariable::Action::DECREMENT,
			 "AdvSceneSwitcher.action.variable.type.decrement"},
			{MacroActionVariable::Action::ROUND_TO_INT,
			 "AdvSceneSwitcher.action.variable.type.roundToInt"},
			{MacroActionVariable::Action::RANDOM_NUMBER,
			 "AdvSceneSwitcher.action.variable.type.randomNumber"},
			{true, ""}, // Separator

			{MacroActionVariable::Action::USER_INPUT,
			 "AdvSceneSwitcher.action.variable.type.askForValue"},
			{MacroActionVariable::Action::ENV_VARIABLE,
			 "AdvSceneSwitcher.action.variable.type.environmentVariable"},
			{true, ""}, // Separator

			{MacroActionVariable::Action::SCENE_ITEM_NAME,
			 "AdvSceneSwitcher.action.variable.type.sceneItemName"},
			{MacroActionVariable::Action::SCENE_ITEM_COUNT,
			 "AdvSceneSwitcher.action.variable.type.sceneItemCount"},
			{true, ""}, // Separator

			{MacroActionVariable::Action::SWAP_VALUES,
			 "AdvSceneSwitcher.action.variable.type.swapValues"},
			{MacroActionVariable::Action::APPEND_VAR,
			 "AdvSceneSwitcher.action.variable.type.appendVar"},

		};

	for (const auto &[action, name] : actions) {
		if (std::holds_alternative<bool>(action)) {
			list->insertSeparator(actions.size());
			continue;
		}

		list->addItem(
			obs_module_text(name.c_str()),
			static_cast<int>(
				std::get<MacroActionVariable::Action>(action)));
	}
}

static inline void populateCaseTypeSelection(QComboBox *list)
{
	const static std::map<MacroActionVariable::CaseType, std::string>
		caseTypes = {
			{MacroActionVariable::CaseType::LOWER_CASE,
			 "AdvSceneSwitcher.action.variable.case.type.lowerCase"},
			{MacroActionVariable::CaseType::UPPER_CASE,
			 "AdvSceneSwitcher.action.variable.case.type.upperCase"},
			{MacroActionVariable::CaseType::CAPITALIZED,
			 "AdvSceneSwitcher.action.variable.case.type.capitalized"},
			{MacroActionVariable::CaseType::START_CASE,
			 "AdvSceneSwitcher.action.variable.case.type.startCase"},
		};

	for (const auto &[type, name] : caseTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(type));
	}
}

static inline void populateDirectionSelection(QComboBox *list)
{
	list->addItems(
		QStringList()
		<< obs_module_text(
			   "AdvSceneSwitcher.action.variable.truncateOrPadDirection.left")
		<< obs_module_text(
			   "AdvSceneSwitcher.action.variable.truncateOrPadDirection.right"));
}

MacroActionVariableEdit::MacroActionVariableEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: QWidget(parent),
	  _variables(new VariableSelection(this)),
	  _variables2(new VariableSelection(this)),
	  _actions(new FilterComboBox(this)),
	  _strValue(new VariableTextEdit(this, 5, 1, 1)),
	  _numValue(new VariableDoubleSpinBox(this)),
	  _segmentIdx(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::CONDITION, false)),
	  _segmentValueStatus(new QLabel()),
	  _segmentValue(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _substringLayout(new QVBoxLayout()),
	  _subStringIndexEntryLayout(new QHBoxLayout()),
	  _subStringRegexEntryLayout(new QHBoxLayout()),
	  _subStringStart(new VariableSpinBox(this)),
	  _subStringSize(new VariableSpinBox(this)),
	  _substringRegex(new RegexConfigWidget(parent)),
	  _regexPattern(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _regexMatchIdx(new VariableSpinBox(this)),
	  _findReplaceLayout(new QHBoxLayout()),
	  _findRegex(new RegexConfigWidget()),
	  _findStr(new VariableTextEdit(this, 10, 1, 1)),
	  _replaceStr(new VariableTextEdit(this, 10, 1, 1)),
	  _mathExpression(new VariableLineEdit(this)),
	  _mathExpressionResult(new QLabel()),
	  _promptLayout(new QHBoxLayout()),
	  _useCustomPrompt(new QCheckBox()),
	  _inputPrompt(new VariableLineEdit(this)),
	  _placeholderLayout(new QHBoxLayout()),
	  _useInputPlaceholder(new QCheckBox()),
	  _inputPlaceholder(new VariableLineEdit(this)),
	  _envVariable(new VariableLineEdit(this)),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _tempVars(new TempVariableSelection(this)),
	  _tempVarsHelp(new HelpIcon(obs_module_text(
		  "AdvSceneSwitcher.action.variable.type.setToTempvar.help"))),
	  _sceneItemIndex(new VariableSpinBox(this)),
	  _direction(new QComboBox()),
	  _stringLength(new VariableSpinBox(this)),
	  _paddingCharSelection(new SingleCharSelection()),
	  _caseType(new FilterComboBox(this)),
	  _randomNumberStart(new VariableDoubleSpinBox(this)),
	  _randomNumberEnd(new VariableDoubleSpinBox(this)),
	  _generateInteger(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.generateInteger"),
		  this)),
	  _randomLayout(new QVBoxLayout()),
	  _jsonQuery(new VariableLineEdit(this)),
	  _jsonQueryHelp(new QLabel(this)),
	  _jsonIndex(new VariableSpinBox(this)),
	  _entryLayout(new QHBoxLayout())
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	_segmentValue->setReadOnly(true);
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
	_inputPrompt->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_sceneItemIndex->setMinimum(1);
	_stringLength->setMaximum(999);
	populateActionSelection(_actions);
	populateDirectionSelection(_direction);
	populateCaseTypeSelection(_caseType);
	_randomNumberStart->setMinimum(-9999999999);
	_randomNumberStart->setMaximum(9999999999);
	_randomNumberEnd->setMinimum(-9999999999);
	_randomNumberEnd->setMaximum(9999999999);
	const QString path = GetThemeTypeName() == "Light"
				     ? ":/res/images/help.svg"
				     : ":/res/images/help_light.svg";
	const QIcon icon(path);
	const QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_jsonQueryHelp->setPixmap(pixmap);
	_jsonQueryHelp->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.variable.type.queryJson.info"));
	_jsonIndex->setMaximum(999);

	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_variables2, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(Variable2Changed(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(textChanged()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(
		_numValue,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(NumValueChanged(const NumberVariable<double> &)));
	QWidget::connect(_segmentIdx,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(SegmentIndexChanged(const IntVariable &)));
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(MacroSegmentOrderChanged()), this,
			 SLOT(MacroSegmentOrderChanged()));
	QWidget::connect(
		_subStringStart,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SubStringStartChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_subStringSize,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SubStringSizeChanged(const NumberVariable<int> &)));
	QWidget::connect(_substringRegex,
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
	QWidget::connect(_mathExpression, SIGNAL(editingFinished()), this,
			 SLOT(MathExpressionChanged()));
	QWidget::connect(_useCustomPrompt, SIGNAL(stateChanged(int)), this,
			 SLOT(UseCustomPromptChanged(int)));
	QWidget::connect(_inputPrompt, SIGNAL(editingFinished()), this,
			 SLOT(InputPromptChanged()));
	QWidget::connect(_useInputPlaceholder, SIGNAL(stateChanged(int)), this,
			 SLOT(UseInputPlaceholderChanged(int)));
	QWidget::connect(_inputPlaceholder, SIGNAL(editingFinished()), this,
			 SLOT(InputPlaceholderChanged()));
	QWidget::connect(_envVariable, SIGNAL(editingFinished()), this,
			 SLOT(EnvVariableChanged()));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_tempVars,
			 SIGNAL(SelectionChanged(const TempVariableRef &)),
			 this, SLOT(SelectionChanged(const TempVariableRef &)));
	QWidget::connect(
		_sceneItemIndex,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SceneItemIndexChanged(const NumberVariable<int> &)));
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
	QWidget::connect(
		_randomNumberStart,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this,
		SLOT(RandomNumberStartChanged(const NumberVariable<double> &)));
	QWidget::connect(
		_randomNumberEnd,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this,
		SLOT(RandomNumberEndChanged(const NumberVariable<double> &)));
	QWidget::connect(_generateInteger, SIGNAL(stateChanged(int)), this,
			 SLOT(GenerateIntegerChanged(int)));
	QWidget::connect(_jsonQuery, SIGNAL(editingFinished()), this,
			 SLOT(JsonQueryChanged()));
	QWidget::connect(
		_jsonIndex,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(JsonIndexChanged(const NumberVariable<int> &)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables},
		{"{{variables2}}", _variables2},
		{"{{actions}}", _actions},
		{"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
		{"{{segmentIndex}}", _segmentIdx},
		{"{{subStringStart}}", _subStringStart},
		{"{{subStringSize}}", _subStringSize},
		{"{{regexMatchIdx}}", _regexMatchIdx},
		{"{{findRegex}}", _findRegex},
		{"{{findStr}}", _findStr},
		{"{{replaceStr}}", _replaceStr},
		{"{{mathExpression}}", _mathExpression},
		{"{{useCustomPrompt}}", _useCustomPrompt},
		{"{{inputPrompt}}", _inputPrompt},
		{"{{useInputPlaceholder}}", _useInputPlaceholder},
		{"{{inputPlaceholder}}", _inputPlaceholder},
		{"{{envVariableName}}", _envVariable},
		{"{{scenes}}", _scenes},
		{"{{tempVars}}", _tempVars},
		{"{{tempVarsHelp}}", _tempVarsHelp},
		{"{{sceneItemIndex}}", _sceneItemIndex},
		{"{{direction}}", _direction},
		{"{{stringLength}}", _stringLength},
		{"{{paddingCharSelection}}", _paddingCharSelection},
		{"{{caseType}}", _caseType},
		{"{{randomNumberStart}}", _randomNumberStart},
		{"{{randomNumberEnd}}", _randomNumberEnd},
		{"{{generateInteger}}", _generateInteger},
		{"{{jsonQuery}}", _jsonQuery},
		{"{{jsonQueryHelp}}", _jsonQueryHelp},
		{"{{jsonIndex}}", _jsonIndex},
	};
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.variable.layout.other"),
		     _entryLayout, widgetPlaceholders);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.substringIndex"),
		_subStringIndexEntryLayout, widgetPlaceholders);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.substringRegex"),
		_subStringRegexEntryLayout, widgetPlaceholders);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.findAndReplace"),
		_findReplaceLayout, widgetPlaceholders, false);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.userInput.customPrompt"),
		_promptLayout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.userInput.placeholder"),
		_placeholderLayout, widgetPlaceholders);

	auto randomLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.randomNumber"),
		randomLayout, widgetPlaceholders);
	_randomLayout->addLayout(randomLayout);
	_randomLayout->addWidget(_generateInteger);

	auto regexConfigLayout = new QHBoxLayout;
	regexConfigLayout->addWidget(_substringRegex);
	regexConfigLayout->addStretch();

	_substringLayout->addLayout(_subStringIndexEntryLayout);
	_substringLayout->addLayout(_subStringRegexEntryLayout);
	_substringLayout->addWidget(_regexPattern);
	_substringLayout->addLayout(regexConfigLayout);

	auto layout = new QVBoxLayout;
	layout->addLayout(_entryLayout);
	layout->addLayout(_substringLayout);
	layout->addWidget(_segmentValueStatus);
	layout->addWidget(_segmentValue);
	layout->addLayout(_findReplaceLayout);
	layout->addWidget(_mathExpressionResult);
	layout->addLayout(_promptLayout);
	layout->addLayout(_placeholderLayout);
	layout->addLayout(_randomLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	UpdateSegmentVariableValue();
	connect(&_timer, SIGNAL(timeout()), this,
		SLOT(UpdateSegmentVariableValue()));
	_timer.start(1500);
}

void MacroActionVariableEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_variables->SetVariable(_entryData->_variable);
	_variables2->SetVariable(_entryData->_variable2);
	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_strValue->setPlainText(_entryData->_strValue);
	_numValue->SetValue(_entryData->_numValue);
	_segmentIdx->SetValue(_entryData->GetSegmentIndexValue() + 1);
	_segmentIdx->SetMacro(_entryData->GetMacro());
	_segmentIdx->SetType(
		_entryData->_action ==
				MacroActionVariable::Action::SET_CONDITION_VALUE
			? MacroSegmentSelection::Type::CONDITION
			: MacroSegmentSelection::Type::ACTION);
	_subStringStart->SetValue(_entryData->_subStringStart);
	_subStringSize->SetValue(_entryData->_subStringSize);
	_substringRegex->SetRegexConfig(_entryData->_subStringRegex);
	_findRegex->SetRegexConfig(_entryData->_findRegex);
	_regexPattern->setPlainText(
		QString::fromStdString(_entryData->_regexPattern));
	_regexMatchIdx->SetValue(_entryData->_regexMatchIdx);
	_findStr->setPlainText(_entryData->_findStr);
	_replaceStr->setPlainText(_entryData->_replaceStr);
	_mathExpression->setText(_entryData->_mathExpression);
	_useCustomPrompt->setChecked(_entryData->_useCustomPrompt);
	_inputPrompt->setText(_entryData->_inputPrompt);
	_useInputPlaceholder->setChecked(_entryData->_useInputPlaceholder);
	_inputPlaceholder->setText(_entryData->_inputPlaceholder);
	_envVariable->setText(_entryData->_envVariableName);
	_scenes->SetScene(_entryData->_scene);
	_tempVars->SetVariable(_entryData->_tempVar);
	_sceneItemIndex->SetValue(_entryData->_sceneItemIndex);
	_direction->setCurrentIndex(static_cast<int>(_entryData->_direction));
	_stringLength->SetValue(_entryData->_stringLength);
	_paddingCharSelection->setText(
		QChar::fromLatin1(_entryData->_paddingChar));
	_caseType->setCurrentIndex(
		_caseType->findData(static_cast<int>(_entryData->_caseType)));
	_randomNumberStart->SetValue(_entryData->_randomNumberStart);
	_randomNumberEnd->SetValue(_entryData->_randomNumberEnd);
	_generateInteger->setChecked(_entryData->_generateInteger);
	_jsonQuery->setText(_entryData->_jsonQuery);
	_jsonIndex->SetValue(_entryData->_jsonIndex);

	SetWidgetVisibility();
}

void MacroActionVariableEdit::VariableChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_variable = GetWeakVariableByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionVariableEdit::Variable2Changed(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_variable2 = GetWeakVariableByQString(text);
}

void MacroActionVariableEdit::ActionChanged(int idx)
{
	if (_loading || !_entryData || idx == -1) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionVariable::Action>(
		_actions->itemData(idx).toInt());

	if (_entryData->_action ==
	    MacroActionVariable::Action::SET_ACTION_VALUE) {
		_segmentIdx->SetType(MacroSegmentSelection::Type::ACTION);
	} else if (_entryData->_action ==
		   MacroActionVariable::Action::SET_CONDITION_VALUE) {
		_segmentIdx->SetType(MacroSegmentSelection::Type::CONDITION);
	}
	SetWidgetVisibility();
}

void MacroActionVariableEdit::StrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::NumValueChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_numValue = value;
}

void MacroActionVariableEdit::SegmentIndexChanged(const IntVariable &val)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetSegmentIndexValue(val - 1);
}

void MacroActionVariableEdit::SetSegmentValueError(const QString &text)
{
	_segmentValueStatus->setText(text);
	_segmentValue->setPlainText("");
	_segmentValue->hide();

	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::UpdateSegmentVariableValue()
{
	if (!_entryData ||
	    !(_entryData->_action ==
		      MacroActionVariable::Action::SET_CONDITION_VALUE ||
	      _entryData->_action ==
		      MacroActionVariable::Action::SET_ACTION_VALUE)) {
		return;
	}

	auto m = _entryData->GetMacro();
	if (!m) {
		return;
	}

	int index = _entryData->GetSegmentIndexValue();
	if (index < 0) {
		SetSegmentValueError(obs_module_text(
			"AdvSceneSwitcher.action.variable.invalidSelection"));
		const QSignalBlocker b(_segmentIdx);
		_segmentIdx->SetValue(0);
		return;
	}

	std::shared_ptr<MacroSegment> segment;
	if (_entryData->_action ==
	    MacroActionVariable::Action::SET_ACTION_VALUE) {
		const auto &actions = m->Actions();
		if (index < (int)actions.size()) {
			segment = actions.at(index);
		}
	} else if (_entryData->_action ==
		   MacroActionVariable::Action::SET_CONDITION_VALUE) {
		const auto &conditions = m->Conditions();
		if (index < (int)conditions.size()) {
			segment = conditions.at(index);
		}
	}

	if (!segment) {
		SetSegmentValueError(obs_module_text(
			"AdvSceneSwitcher.action.variable.invalidSelection"));
		return;
	}

	if (!SupportsVariableValue(segment.get())) {
		std::string type;
		QString fmt;

		if (_entryData->_action ==
		    MacroActionVariable::Action::SET_ACTION_VALUE) {
			type = MacroActionFactory::GetActionName(
				segment->GetId());
			fmt = QString(obs_module_text(
				"AdvSceneSwitcher.action.variable.actionNoVariableSupport"));
		} else if (_entryData->_action ==
			   MacroActionVariable::Action::SET_CONDITION_VALUE) {
			type = MacroConditionFactory::GetConditionName(
				segment->GetId());
			fmt = QString(obs_module_text(
				"AdvSceneSwitcher.action.variable.conditionNoVariableSupport"));
		}
		SetSegmentValueError(
			fmt.arg(QString(obs_module_text(type.c_str()))));
		return;
	}

	_segmentValueStatus->setText(obs_module_text(
		"AdvSceneSwitcher.action.variable.currentSegmentValue"));
	_segmentValue->show();

	// Only update the text if the value changed to prevent possible text
	// selections being lost
	auto previousText = _segmentValue->toPlainText();
	auto newText = QString::fromStdString(segment->GetVariableValue());
	if (newText != previousText) {
		_segmentValue->setPlainText(newText);
	}

	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::MacroSegmentOrderChanged()
{
	const QSignalBlocker b(_segmentIdx);
	_segmentIdx->SetValue(_entryData->GetSegmentIndexValue() + 1);
}

void MacroActionVariableEdit::SubStringStartChanged(
	const NumberVariable<int> &start)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_subStringStart = start;
}

void MacroActionVariableEdit::SubStringSizeChanged(
	const NumberVariable<int> &size)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_subStringSize = size;
}

void MacroActionVariableEdit::SubStringRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_subStringRegex = conf;

	SetWidgetVisibility();
}

void MacroActionVariableEdit::RegexPatternChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regexPattern = _regexPattern->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::RegexMatchIdxChanged(
	const NumberVariable<int> &index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regexMatchIdx = index;
}

void MacroActionVariableEdit::FindStrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_findStr = _findStr->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::FindRegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_findRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::ReplaceStrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_replaceStr = _replaceStr->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::MathExpressionChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_mathExpression = _mathExpression->text().toStdString();

	// In case of invalid expression display an error
	auto result = EvalMathExpression(_entryData->_mathExpression);
	auto hasError = std::holds_alternative<std::string>(result);
	if (hasError) {
		_mathExpressionResult->setText(
			QString::fromStdString(std::get<std::string>(result)));
	}
	_mathExpressionResult->setVisible(hasError);

	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::UseCustomPromptChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useCustomPrompt = value;
	SetWidgetVisibility();
}

void MacroActionVariableEdit::InputPromptChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_inputPrompt = _inputPrompt->text().toStdString();
}

void MacroActionVariableEdit::UseInputPlaceholderChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useInputPlaceholder = value;
	SetWidgetVisibility();
}

void MacroActionVariableEdit::InputPlaceholderChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_inputPlaceholder = _inputPlaceholder->text().toStdString();
}

void MacroActionVariableEdit::EnvVariableChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_envVariableName = _envVariable->text().toStdString();
}

void MacroActionVariableEdit::SceneChanged(const SceneSelection &scene)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = scene;
}

void MacroActionVariableEdit::SelectionChanged(const TempVariableRef &var)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_tempVar = var;
	SetWidgetVisibility();
}

void MacroActionVariableEdit::SceneItemIndexChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_sceneItemIndex = value;
}

void MacroActionVariableEdit::DirectionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_direction =
		static_cast<MacroActionVariable::Direction>(value);
}

void MacroActionVariableEdit::StringLengthChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_stringLength = value;
}

void MacroActionVariableEdit::CharSelectionChanged(const QString &character)
{
	GUARD_LOADING_AND_LOCK();
	if (character.isEmpty()) {
		_entryData->_paddingChar = ' ';
	} else {
		_entryData->_paddingChar = character.toStdString().at(0);
	}
}

void MacroActionVariableEdit::CaseTypeChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_caseType = static_cast<MacroActionVariable::CaseType>(
		_caseType->itemData(index).toInt());
}

void MacroActionVariableEdit::RandomNumberStartChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_randomNumberStart = value;
}

void MacroActionVariableEdit::RandomNumberEndChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_randomNumberEnd = value;
}

void MacroActionVariableEdit::GenerateIntegerChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_generateInteger = value;
}

void MacroActionVariableEdit::JsonQueryChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_jsonQuery = _jsonQuery->text().toStdString();
}

void MacroActionVariableEdit::JsonIndexChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_jsonIndex = value;
}

void MacroActionVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables},
		{"{{variables2}}", _variables2},
		{"{{actions}}", _actions},
		{"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
		{"{{segmentIndex}}", _segmentIdx},
		{"{{mathExpression}}", _mathExpression},
		{"{{envVariableName}}", _envVariable},
		{"{{scenes}}", _scenes},
		{"{{tempVars}}", _tempVars},
		{"{{tempVarsHelp}}", _tempVarsHelp},
		{"{{sceneItemIndex}}", _sceneItemIndex},
		{"{{direction}}", _direction},
		{"{{stringLength}}", _stringLength},
		{"{{paddingCharSelection}}", _paddingCharSelection},
		{"{{caseType}}", _caseType},
		{"{{jsonQuery}}", _jsonQuery},
		{"{{jsonQueryHelp}}", _jsonQueryHelp},
		{"{{jsonIndex}}", _jsonIndex},
	};

	const char *layoutString = "";
	if (_entryData->_action == MacroActionVariable::Action::PAD) {
		layoutString = obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.pad");
	} else if (_entryData->_action ==
		   MacroActionVariable::Action::TRUNCATE) {
		layoutString = obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.truncate");
	} else {
		layoutString = obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.other");
	}

	for (const auto &[_, widget] : widgetPlaceholders) {
		_entryLayout->removeWidget(widget);
	}

	ClearLayout(_entryLayout);
	PlaceWidgets(layoutString, _entryLayout, widgetPlaceholders);

	if (_entryData->_action == MacroActionVariable::Action::SET_VALUE ||
	    _entryData->_action == MacroActionVariable::Action::APPEND ||
	    _entryData->_action ==
		    MacroActionVariable::Action::MATH_EXPRESSION ||
	    _entryData->_action == MacroActionVariable::Action::ENV_VARIABLE ||
	    _entryData->_action == MacroActionVariable::Action::STRING_LENGTH ||
	    _entryData->_action == MacroActionVariable::Action::EXTRACT_JSON ||
	    _entryData->_action == MacroActionVariable::Action::QUERY_JSON) {
		RemoveStretchIfPresent(_entryLayout);
	} else {
		AddStretchIfNecessary(_entryLayout);
	}

	_variables2->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::APPEND_VAR ||
		_entryData->_action ==
			MacroActionVariable::Action::SWAP_VALUES);
	_strValue->setVisible(
		_entryData->_action == MacroActionVariable::Action::SET_VALUE ||
		_entryData->_action == MacroActionVariable::Action::APPEND ||
		_entryData->_action ==
			MacroActionVariable::Action::STRING_LENGTH ||
		_entryData->_action ==
			MacroActionVariable::Action::EXTRACT_JSON);
	_numValue->setVisible(
		_entryData->_action == MacroActionVariable::Action::INCREMENT ||
		_entryData->_action == MacroActionVariable::Action::DECREMENT);
	_segmentValueStatus->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::SET_ACTION_VALUE ||
		_entryData->_action ==
			MacroActionVariable::Action::SET_CONDITION_VALUE);
	_segmentValue->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::SET_ACTION_VALUE ||
		_entryData->_action ==
			MacroActionVariable::Action::SET_CONDITION_VALUE);
	_segmentIdx->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::SET_ACTION_VALUE ||
		_entryData->_action ==
			MacroActionVariable::Action::SET_CONDITION_VALUE);
	SetLayoutVisible(_substringLayout,
			 _entryData->_action ==
				 MacroActionVariable::Action::SUBSTRING);
	if (_entryData->_action == MacroActionVariable::Action::SUBSTRING) {
		bool showRegex = _entryData->_subStringRegex.Enabled();
		SetLayoutVisible(_subStringIndexEntryLayout, !showRegex);
		SetLayoutVisible(_subStringRegexEntryLayout, showRegex);
		_regexPattern->setVisible(showRegex);
	}
	SetLayoutVisible(_findReplaceLayout,
			 _entryData->_action ==
				 MacroActionVariable::Action::FIND_AND_REPLACE);
	_mathExpression->setVisible(
		_entryData->_action ==
		MacroActionVariable::Action::MATH_EXPRESSION);
	_mathExpressionResult->hide();
	SetLayoutVisible(_promptLayout,
			 _entryData->_action ==
				 MacroActionVariable::Action::USER_INPUT);
	_inputPrompt->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::USER_INPUT &&
		_entryData->_useCustomPrompt);
	if (_entryData->_useCustomPrompt) {
		RemoveStretchIfPresent(_promptLayout);
	} else {
		AddStretchIfNecessary(_promptLayout);
	}
	SetLayoutVisible(
		_placeholderLayout,
		_entryData->_action ==
				MacroActionVariable::Action::USER_INPUT &&
			_entryData->_useCustomPrompt);
	_useInputPlaceholder->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::USER_INPUT &&
		_entryData->_useCustomPrompt);
	_inputPlaceholder->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::USER_INPUT &&
		_entryData->_useCustomPrompt &&
		_entryData->_useInputPlaceholder);
	if (_entryData->_useInputPlaceholder) {
		RemoveStretchIfPresent(_placeholderLayout);
	} else {
		AddStretchIfNecessary(_placeholderLayout);
	}
	_envVariable->setVisible(_entryData->_action ==
				 MacroActionVariable::Action::ENV_VARIABLE);
	_scenes->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::SCENE_ITEM_COUNT ||
		_entryData->_action ==
			MacroActionVariable::Action::SCENE_ITEM_NAME);
	_tempVars->setVisible(_entryData->_action ==
			      MacroActionVariable::Action::SET_TO_TEMPVAR);
	_tempVarsHelp->setVisible(
		_entryData->_action ==
			MacroActionVariable::Action::SET_TO_TEMPVAR &&
		!_entryData->_tempVar.HasValidID());
	_sceneItemIndex->setVisible(
		_entryData->_action ==
		MacroActionVariable::Action::SCENE_ITEM_NAME);
	_direction->setVisible(
		_entryData->_action == MacroActionVariable::Action::PAD ||
		_entryData->_action == MacroActionVariable::Action::TRUNCATE);
	_stringLength->setVisible(
		_entryData->_action == MacroActionVariable::Action::PAD ||
		_entryData->_action == MacroActionVariable::Action::TRUNCATE);
	_paddingCharSelection->setVisible(_entryData->_action ==
					  MacroActionVariable::Action::PAD);
	_caseType->setVisible(_entryData->_action ==
			      MacroActionVariable::Action::CHANGE_CASE);
	SetLayoutVisible(_randomLayout,
			 _entryData->_action ==
				 MacroActionVariable::Action::RANDOM_NUMBER);
	_jsonQuery->setVisible(_entryData->_action ==
			       MacroActionVariable::Action::QUERY_JSON);
	_jsonQueryHelp->setVisible(_entryData->_action ==
				   MacroActionVariable::Action::QUERY_JSON);
	_jsonIndex->setVisible(_entryData->_action ==
			       MacroActionVariable::Action::ARRAY_JSON);

	adjustSize();
	updateGeometry();
}

} // namespace advss
