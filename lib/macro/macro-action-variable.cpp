#include "macro-action-variable.hpp"
#include "advanced-scene-switcher.hpp"
#include "layout-helpers.hpp"
#include "math-helpers.hpp"
#include "macro-condition-edit.hpp"
#include "macro.hpp"
#include "non-modal-dialog.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionVariable::id = "variable";

bool MacroActionVariable::_registered = MacroActionFactory::Register(
	MacroActionVariable::id,
	{MacroActionVariable::Create, MacroActionVariableEdit::Create,
	 "AdvSceneSwitcher.action.variable"});

const static std::map<MacroActionVariable::Type, std::string> actionTypes = {
	{MacroActionVariable::Type::SET_FIXED_VALUE,
	 "AdvSceneSwitcher.action.variable.type.set"},
	{MacroActionVariable::Type::APPEND,
	 "AdvSceneSwitcher.action.variable.type.append"},
	{MacroActionVariable::Type::APPEND_VAR,
	 "AdvSceneSwitcher.action.variable.type.appendVar"},
	{MacroActionVariable::Type::INCREMENT,
	 "AdvSceneSwitcher.action.variable.type.increment"},
	{MacroActionVariable::Type::DECREMENT,
	 "AdvSceneSwitcher.action.variable.type.decrement"},
	{MacroActionVariable::Type::SET_CONDITION_VALUE,
	 "AdvSceneSwitcher.action.variable.type.setConditionValue"},
	{MacroActionVariable::Type::SET_ACTION_VALUE,
	 "AdvSceneSwitcher.action.variable.type.setActionValue"},
	{MacroActionVariable::Type::ROUND_TO_INT,
	 "AdvSceneSwitcher.action.variable.type.roundToInt"},
	{MacroActionVariable::Type::SUBSTRING,
	 "AdvSceneSwitcher.action.variable.type.subString"},
	{MacroActionVariable::Type::FIND_AND_REPLACE,
	 "AdvSceneSwitcher.action.variable.type.findAndReplace"},
	{MacroActionVariable::Type::MATH_EXPRESSION,
	 "AdvSceneSwitcher.action.variable.type.mathExpression"},
	{MacroActionVariable::Type::USER_INPUT,
	 "AdvSceneSwitcher.action.variable.type.askForValue"},
	{MacroActionVariable::Type::ENV_VARIABLE,
	 "AdvSceneSwitcher.action.variable.type.environmentVariable"},
	{MacroActionVariable::Type::SCENE_ITEM_COUNT,
	 "AdvSceneSwitcher.action.variable.type.sceneItemCount"},
	{MacroActionVariable::Type::STRING_LENGTH,
	 "AdvSceneSwitcher.action.variable.type.stringLength"},
	{MacroActionVariable::Type::EXTRACT_JSON,
	 "AdvSceneSwitcher.action.variable.type.extractJson"},
	{MacroActionVariable::Type::SET_TO_TEMPVAR,
	 "AdvSceneSwitcher.action.variable.type.setToTempvar"},
	{MacroActionVariable::Type::SCENE_ITEM_NAME,
	 "AdvSceneSwitcher.action.variable.type.sceneItemName"},
	{MacroActionVariable::Type::PAD,
	 "AdvSceneSwitcher.action.variable.type.padValue"},
	{MacroActionVariable::Type::TRUNCATE,
	 "AdvSceneSwitcher.action.variable.type.truncateValue"},
	{MacroActionVariable::Type::SWAP_VALUES,
	 "AdvSceneSwitcher.action.variable.type.swapValues"},
};

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
	try {
		if (_subStringSize == 0) {
			var->SetValue(curValue.substr(_subStringStart));
			return;
		}
		var->SetValue(curValue.substr(_subStringStart, _subStringSize));
	} catch (const std::out_of_range &) {
		vblog(LOG_WARNING,
		      "invalid start index \"%d\" selected for substring of \"%s\" of variable \"%s\"",
		      _subStringStart, curValue.c_str(), var->Name().c_str());
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
	for (int idx = 0; idx < _regexMatchIdx; idx++) {
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
	QString resultString = QString::fromStdString(value);
	QString replacement = QString::fromStdString(_replaceStr);
	auto regex = _findRegex.GetRegularExpression(_findStr);
	auto matchIterator = regex.globalMatch(QString::fromStdString(value));
	int offset = 0;
	while (matchIterator.hasNext()) {
		QRegularExpressionMatch match = matchIterator.next();
		resultString.replace(match.capturedStart() + offset,
				     match.capturedLength(), replacement);
		offset += replacement.length() - match.capturedLength();
	}
	var->SetValue(resultString.toStdString());
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

	switch (_type) {
	case Type::SET_FIXED_VALUE:
		var->SetValue(_strValue);
		break;
	case Type::APPEND:
		apppend(*var, _strValue);
		break;
	case Type::APPEND_VAR: {
		auto var2 = _variable2.lock();
		if (!var2) {
			return true;
		}
		apppend(*var, var2->Value());
		break;
	}
	case Type::INCREMENT:
		modifyNumValue(*var, _numValue, true);
		break;
	case Type::DECREMENT:
		modifyNumValue(*var, _numValue, false);
		break;
	case Type::SET_CONDITION_VALUE:
	case Type::SET_ACTION_VALUE: {
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
	case Type::ROUND_TO_INT: {
		auto curValue = var->DoubleValue();
		if (!curValue.has_value()) {
			return true;
		}
		var->SetValue(std::to_string(int(std::round(*curValue))));
		return true;
	}
	case Type::SUBSTRING: {
		if (_subStringRegex.Enabled()) {
			HandleRegexSubString(var.get());
			return true;
		}
		HandleIndexSubString(var.get());
		return true;
	}
	case Type::FIND_AND_REPLACE: {
		HandleFindAndReplace(var.get());
		return true;
	}
	case Type::MATH_EXPRESSION: {
		HandleMathExpression(var.get());
		return true;
	}
	case Type::USER_INPUT: {
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
	case Type::ENV_VARIABLE: {
		auto value = std::getenv(_envVariableName.c_str());
		if (value) {
			var->SetValue(value);
		}
		return true;
	}
	case Type::SCENE_ITEM_COUNT: {
		var->SetValue(GetSceneItemCount(_scene.GetScene(false)));
		return true;
	}
	case Type::STRING_LENGTH: {
		var->SetValue(std::string(_strValue).length());
		return true;
	}
	case Type::EXTRACT_JSON: {
		auto value = GetJsonField(var->Value(), _strValue);
		if (!value.has_value()) {
			return true;
		}
		var->SetValue(*value);
		return true;
	}
	case Type::SET_TO_TEMPVAR: {
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
	case Type::SCENE_ITEM_NAME:
		SetToSceneItemName(var.get());
		return true;
	case Type::PAD:
		var->SetValue(pad(var->Value(), _direction, _stringLength,
				  _paddingChar));
		return true;
	case Type::TRUNCATE:
		var->SetValue(
			truncate(var->Value(), _direction, _stringLength));
		return true;
	case Type::SWAP_VALUES: {
		auto var2 = _variable2.lock();
		if (!var2) {
			return true;
		}

		auto tempValue = var->Value();
		var->SetValue(var2->Value());
		var2->SetValue(tempValue);
		return true;
	}
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
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	obs_data_set_int(obj, "segmentIdx", GetSegmentIndexValue());
	obs_data_set_int(obj, "subStringStart", _subStringStart);
	obs_data_set_int(obj, "subStringSize", _subStringSize);
	obs_data_set_string(obj, "regexPattern", _regexPattern.c_str());
	obs_data_set_int(obj, "regexMatchIdx", _regexMatchIdx);
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
	_numValue = obs_data_get_double(obj, "numValue");
	_type = static_cast<Type>(obs_data_get_int(obj, "condition"));
	_segmentIdxLoadValue = obs_data_get_int(obj, "segmentIdx");
	_subStringStart = obs_data_get_int(obj, "subStringStart");
	_subStringSize = obs_data_get_int(obj, "subStringSize");
	_subStringRegex.Load(obj);
	_regexPattern = obs_data_get_string(obj, "regexPattern");
	_regexMatchIdx = obs_data_get_int(obj, "regexMatchIdx");
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
	if (_type == Type::SET_CONDITION_VALUE) {
		if (value < (int)m->Conditions().size()) {
			segment = m->Conditions().at(value);
		}
	} else if (_type == Type::SET_ACTION_VALUE) {
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

	if (_type == Type::SET_CONDITION_VALUE) {
		auto it = std::find(m->Conditions().begin(),
				    m->Conditions().end(), segment);
		if (it != m->Conditions().end()) {
			return std::distance(m->Conditions().begin(), it);
		}
		return -1;
	} else if (_type == Type::SET_ACTION_VALUE) {
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
}

void MacroActionVariable::DecrementCurrentSegmentVariableRef()
{
	auto segment = _macroSegment.lock();
	if (!segment) {
		return;
	}

	DecrementVariableRef(segment.get());
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (const auto &[action, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
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
	  _numValue(new QDoubleSpinBox()),
	  _segmentIdx(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::CONDITION, false)),
	  _segmentValueStatus(new QLabel()),
	  _segmentValue(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _substringLayout(new QVBoxLayout()),
	  _subStringIndexEntryLayout(new QHBoxLayout()),
	  _subStringRegexEntryLayout(new QHBoxLayout()),
	  _subStringStart(new QSpinBox()),
	  _subStringSize(new QSpinBox()),
	  _substringRegex(new RegexConfigWidget(parent)),
	  _regexPattern(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _regexMatchIdx(new QSpinBox()),
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
	  _tempVarsHelp(new QLabel()),
	  _sceneItemIndex(new VariableSpinBox()),
	  _direction(new QComboBox()),
	  _stringLength(new VariableSpinBox()),
	  _paddingCharSelection(new SingleCharSelection()),
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
	_inputPrompt->setSizePolicy(QSizePolicy::MinimumExpanding,
				    QSizePolicy::Preferred);
	_sceneItemIndex->setMinimum(1);
	_stringLength->setMaximum(999);
	populateTypeSelection(_actions);
	populateDirectionSelection(_direction);

	QString path = GetThemeTypeName() == "Light"
			       ? ":/res/images/help.svg"
			       : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_tempVarsHelp->setPixmap(pixmap);
	_tempVarsHelp->setToolTip(obs_module_text(
		"AdvSceneSwitcher.action.variable.type.setToTempvar.help"));

	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_variables2, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(Variable2Changed(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(textChanged()), this,
			 SLOT(StrValueChanged()));
	QWidget::connect(_numValue, SIGNAL(valueChanged(double)), this,
			 SLOT(NumValueChanged(double)));
	QWidget::connect(_segmentIdx,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(SegmentIndexChanged(const IntVariable &)));
	QWidget::connect(window(), SIGNAL(MacroSegmentOrderChanged()), this,
			 SLOT(MacroSegmentOrderChanged()));
	QWidget::connect(_subStringStart, SIGNAL(valueChanged(int)), this,
			 SLOT(SubStringStartChanged(int)));
	QWidget::connect(_subStringSize, SIGNAL(valueChanged(int)), this,
			 SLOT(SubStringSizeChanged(int)));
	QWidget::connect(_substringRegex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(SubStringRegexChanged(const RegexConfig &)));
	QWidget::connect(_regexPattern, SIGNAL(textChanged()), this,
			 SLOT(RegexPatternChanged()));
	QWidget::connect(_regexMatchIdx, SIGNAL(valueChanged(int)), this,
			 SLOT(RegexMatchIdxChanged(int)));
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
	};
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.variable.entry.other"),
		_entryLayout, widgetPlaceholders);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.substringIndex"),
		_subStringIndexEntryLayout, widgetPlaceholders);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.substringRegex"),
		_subStringRegexEntryLayout, widgetPlaceholders);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.findAndReplace"),
		_findReplaceLayout, widgetPlaceholders, false);

	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.userInput.customPrompt"),
		_promptLayout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.userInput.placeholder"),
		_placeholderLayout, widgetPlaceholders);

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
		_actions->findData(static_cast<int>(_entryData->_type)));
	_strValue->setPlainText(_entryData->_strValue);
	_numValue->setValue(_entryData->_numValue);
	_segmentIdx->SetValue(_entryData->GetSegmentIndexValue() + 1);
	_segmentIdx->SetMacro(_entryData->GetMacro());
	_segmentIdx->SetType(
		_entryData->_type ==
				MacroActionVariable::Type::SET_CONDITION_VALUE
			? MacroSegmentSelection::Type::CONDITION
			: MacroSegmentSelection::Type::ACTION);
	_subStringStart->setValue(_entryData->_subStringStart + 1);
	_subStringSize->setValue(_entryData->_subStringSize);
	_substringRegex->SetRegexConfig(_entryData->_subStringRegex);
	_findRegex->SetRegexConfig(_entryData->_findRegex);
	_regexPattern->setPlainText(
		QString::fromStdString(_entryData->_regexPattern));
	_regexMatchIdx->setValue(_entryData->_regexMatchIdx + 1);
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
	SetWidgetVisibility();
}

void MacroActionVariableEdit::VariableChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_variable = GetWeakVariableByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionVariableEdit::Variable2Changed(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_variable2 = GetWeakVariableByQString(text);
}

void MacroActionVariableEdit::ActionChanged(int idx)
{
	if (_loading || !_entryData || idx == -1) {
		return;
	}

	auto lock = LockContext();
	_entryData->_type = static_cast<MacroActionVariable::Type>(
		_actions->itemData(idx).toInt());

	if (_entryData->_type == MacroActionVariable::Type::SET_ACTION_VALUE) {
		_segmentIdx->SetType(MacroSegmentSelection::Type::ACTION);
	} else if (_entryData->_type ==
		   MacroActionVariable::Type::SET_CONDITION_VALUE) {
		_segmentIdx->SetType(MacroSegmentSelection::Type::CONDITION);
	}
	SetWidgetVisibility();
}

void MacroActionVariableEdit::StrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::NumValueChanged(double val)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_numValue = val;
}

void MacroActionVariableEdit::SegmentIndexChanged(const IntVariable &val)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
	    !(_entryData->_type ==
		      MacroActionVariable::Type::SET_CONDITION_VALUE ||
	      _entryData->_type ==
		      MacroActionVariable::Type::SET_ACTION_VALUE)) {
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
	if (_entryData->_type == MacroActionVariable::Type::SET_ACTION_VALUE) {
		const auto &actions = m->Actions();
		if (index < (int)actions.size()) {
			segment = actions.at(index);
		}
	} else if (_entryData->_type ==
		   MacroActionVariable::Type::SET_CONDITION_VALUE) {
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

		if (_entryData->_type ==
		    MacroActionVariable::Type::SET_ACTION_VALUE) {
			type = MacroActionFactory::GetActionName(
				segment->GetId());
			fmt = QString(obs_module_text(
				"AdvSceneSwitcher.action.variable.actionNoVariableSupport"));
		} else if (_entryData->_type ==
			   MacroActionVariable::Type::SET_CONDITION_VALUE) {
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

void MacroActionVariableEdit::SubStringStartChanged(int val)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_subStringStart = val - 1;
}

void MacroActionVariableEdit::SubStringSizeChanged(int val)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_subStringSize = val;
}

void MacroActionVariableEdit::SubStringRegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_subStringRegex = conf;

	SetWidgetVisibility();
}

void MacroActionVariableEdit::RegexPatternChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regexPattern = _regexPattern->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::RegexMatchIdxChanged(int val)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_regexMatchIdx = val - 1;
}

void MacroActionVariableEdit::FindStrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_findStr = _findStr->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::FindRegexChanged(const RegexConfig &conf)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_findRegex = conf;
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::ReplaceStrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_replaceStr = _replaceStr->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::MathExpressionChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_useCustomPrompt = value;
	SetWidgetVisibility();
}

void MacroActionVariableEdit::InputPromptChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_inputPrompt = _inputPrompt->text().toStdString();
}

void MacroActionVariableEdit::UseInputPlaceholderChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_useInputPlaceholder = value;
	SetWidgetVisibility();
}

void MacroActionVariableEdit::InputPlaceholderChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_inputPlaceholder = _inputPlaceholder->text().toStdString();
}

void MacroActionVariableEdit::EnvVariableChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_envVariableName = _envVariable->text().toStdString();
}

void MacroActionVariableEdit::SceneChanged(const SceneSelection &scene)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_scene = scene;
}

void MacroActionVariableEdit::SelectionChanged(const TempVariableRef &var)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_tempVar = var;
	SetWidgetVisibility();
}

void MacroActionVariableEdit::SceneItemIndexChanged(
	const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_sceneItemIndex = value;
}

void MacroActionVariableEdit::DirectionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_direction =
		static_cast<MacroActionVariable::Direction>(value);
}

void MacroActionVariableEdit::StringLengthChanged(
	const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_stringLength = value;
}

void MacroActionVariableEdit::CharSelectionChanged(const QString &character)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	if (character.isEmpty()) {
		_entryData->_paddingChar = ' ';
	} else {
		_entryData->_paddingChar = character.toStdString().at(0);
	}
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
	};

	const char *layoutString = "";
	if (_entryData->_type == MacroActionVariable::Type::PAD) {
		layoutString = obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.pad");
	} else if (_entryData->_type == MacroActionVariable::Type::TRUNCATE) {
		layoutString = obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.truncate");
	} else {
		layoutString = obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.other");
	}

	for (const auto &[_, widget] : widgetPlaceholders) {
		_entryLayout->removeWidget(widget);
	}

	ClearLayout(_entryLayout);
	PlaceWidgets(layoutString, _entryLayout, widgetPlaceholders);

	if (_entryData->_type == MacroActionVariable::Type::SET_FIXED_VALUE ||
	    _entryData->_type == MacroActionVariable::Type::APPEND ||
	    _entryData->_type == MacroActionVariable::Type::MATH_EXPRESSION ||
	    _entryData->_type == MacroActionVariable::Type::ENV_VARIABLE ||
	    _entryData->_type == MacroActionVariable::Type::STRING_LENGTH ||
	    _entryData->_type == MacroActionVariable::Type::EXTRACT_JSON) {
		RemoveStretchIfPresent(_entryLayout);
	} else {
		AddStretchIfNecessary(_entryLayout);
	}

	_variables2->setVisible(
		_entryData->_type == MacroActionVariable::Type::APPEND_VAR ||
		_entryData->_type == MacroActionVariable::Type::SWAP_VALUES);
	_strValue->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SET_FIXED_VALUE ||
		_entryData->_type == MacroActionVariable::Type::APPEND ||
		_entryData->_type == MacroActionVariable::Type::STRING_LENGTH ||
		_entryData->_type == MacroActionVariable::Type::EXTRACT_JSON);
	_numValue->setVisible(
		_entryData->_type == MacroActionVariable::Type::INCREMENT ||
		_entryData->_type == MacroActionVariable::Type::DECREMENT);
	_segmentValueStatus->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SET_ACTION_VALUE ||
		_entryData->_type ==
			MacroActionVariable::Type::SET_CONDITION_VALUE);
	_segmentValue->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SET_ACTION_VALUE ||
		_entryData->_type ==
			MacroActionVariable::Type::SET_CONDITION_VALUE);
	_segmentIdx->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SET_ACTION_VALUE ||
		_entryData->_type ==
			MacroActionVariable::Type::SET_CONDITION_VALUE);
	SetLayoutVisible(_substringLayout,
			 _entryData->_type ==
				 MacroActionVariable::Type::SUBSTRING);
	if (_entryData->_type == MacroActionVariable::Type::SUBSTRING) {
		bool showRegex = _entryData->_subStringRegex.Enabled();
		SetLayoutVisible(_subStringIndexEntryLayout, !showRegex);
		SetLayoutVisible(_subStringRegexEntryLayout, showRegex);
		_regexPattern->setVisible(showRegex);
	}
	SetLayoutVisible(_findReplaceLayout,
			 _entryData->_type ==
				 MacroActionVariable::Type::FIND_AND_REPLACE);
	_mathExpression->setVisible(_entryData->_type ==
				    MacroActionVariable::Type::MATH_EXPRESSION);
	_mathExpressionResult->hide();
	SetLayoutVisible(_promptLayout,
			 _entryData->_type ==
				 MacroActionVariable::Type::USER_INPUT);
	_inputPrompt->setVisible(
		_entryData->_type == MacroActionVariable::Type::USER_INPUT &&
		_entryData->_useCustomPrompt);
	if (_entryData->_useCustomPrompt) {
		RemoveStretchIfPresent(_promptLayout);
	} else {
		AddStretchIfNecessary(_promptLayout);
	}
	SetLayoutVisible(
		_placeholderLayout,
		_entryData->_type == MacroActionVariable::Type::USER_INPUT &&
			_entryData->_useCustomPrompt);
	_useInputPlaceholder->setVisible(
		_entryData->_type == MacroActionVariable::Type::USER_INPUT &&
		_entryData->_useCustomPrompt);
	_inputPlaceholder->setVisible(
		_entryData->_type == MacroActionVariable::Type::USER_INPUT &&
		_entryData->_useCustomPrompt &&
		_entryData->_useInputPlaceholder);
	if (_entryData->_useInputPlaceholder) {
		RemoveStretchIfPresent(_placeholderLayout);
	} else {
		AddStretchIfNecessary(_placeholderLayout);
	}
	_envVariable->setVisible(_entryData->_type ==
				 MacroActionVariable::Type::ENV_VARIABLE);
	_scenes->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SCENE_ITEM_COUNT ||
		_entryData->_type ==
			MacroActionVariable::Type::SCENE_ITEM_NAME);
	_tempVars->setVisible(_entryData->_type ==
			      MacroActionVariable::Type::SET_TO_TEMPVAR);
	_tempVarsHelp->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SET_TO_TEMPVAR &&
		!_entryData->_tempVar.HasValidID());
	_sceneItemIndex->setVisible(_entryData->_type ==
				    MacroActionVariable::Type::SCENE_ITEM_NAME);
	_direction->setVisible(
		_entryData->_type == MacroActionVariable::Type::PAD ||
		_entryData->_type == MacroActionVariable::Type::TRUNCATE);
	_stringLength->setVisible(
		_entryData->_type == MacroActionVariable::Type::PAD ||
		_entryData->_type == MacroActionVariable::Type::TRUNCATE);
	_paddingCharSelection->setVisible(_entryData->_type ==
					  MacroActionVariable::Type::PAD);
	adjustSize();
	updateGeometry();
}

} // namespace advss
