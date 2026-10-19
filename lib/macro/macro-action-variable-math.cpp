#include "macro-action-variable-math.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "math-helpers.hpp"

#include <random>

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{40,
	 {
		 {Action::MATH_EXPRESSION,
		  "AdvSceneSwitcher.action.variable.type.mathExpression",
		  "AdvSceneSwitcher.action.variable.layout.mathExpression",
		  true},
		 {Action::INCREMENT,
		  "AdvSceneSwitcher.action.variable.type.increment",
		  "AdvSceneSwitcher.action.variable.layout.numValue"},
		 {Action::DECREMENT,
		  "AdvSceneSwitcher.action.variable.type.decrement",
		  "AdvSceneSwitcher.action.variable.layout.numValue"},
		 {Action::ROUND_TO_INT,
		  "AdvSceneSwitcher.action.variable.type.roundToInt"},
		 {Action::RANDOM_NUMBER,
		  "AdvSceneSwitcher.action.variable.type.randomNumber"},
		 {Action::RANDOM_LIST_VALUE,
		  "AdvSceneSwitcher.action.variable.type.randomListValue"},
	 },
	 []() { return std::make_unique<VariableActionMath>(); },
	 VariableActionMathEdit::Create});

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

std::unique_ptr<VariableActionGroup> VariableActionMath::Copy() const
{
	return std::make_unique<VariableActionMath>(*this);
}

void VariableActionMath::Save(obs_data_t *obj,
			      const MacroActionVariable &) const
{
	_numValue.Save(obj, "numValue");
	_mathExpression.Save(obj, "mathExpression");
	_randomNumberStart.Save(obj, "randomNumberStart");
	_randomNumberEnd.Save(obj, "randomNumberEnd");
	obs_data_set_bool(obj, "generateInteger", _generateInteger);
	_randomValues.Save(obj, "randomValues", "value");
	obs_data_set_bool(obj, "allowRepeatValues", _allowRepeatValues);
}

void VariableActionMath::Load(obs_data_t *obj, MacroActionVariable &)
{
	_mathExpression.Load(obj, "mathExpression");

	// Convert old data format
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, "version")) {
		_numValue = obs_data_get_double(obj, "numValue");
	} else {
		_numValue.Load(obj, "numValue");
	}

	_randomNumberStart.Load(obj, "randomNumberStart");
	_randomNumberEnd.Load(obj, "randomNumberEnd");
	_generateInteger = obs_data_get_bool(obj, "generateInteger");
	_randomValues.Load(obj, "randomValues", "value");
	_allowRepeatValues = obs_data_get_bool(obj, "allowRepeatValues");
}

void VariableActionMath::ResolveVariables()
{
	_mathExpression.ResolveVariables();
}

void VariableActionMath::HandleMathExpression(Variable &var)
{
	auto result = EvalMathExpression(_mathExpression);
	if (std::holds_alternative<std::string>(result)) {
		blog(LOG_WARNING, "%s", std::get<std::string>(result).c_str());
		return;
	}
	var.SetValue(std::get<double>(result));
}

void VariableActionMath::GenerateRandomNumber(Variable &var)
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
		var.SetValue(dis(gen));
	} else {
		std::uniform_real_distribution<double> dis(start, end);
		var.SetValue(dis(gen));
	}
}

void VariableActionMath::PickRandomValue(Variable &var)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static const uint8_t maxIter = 255;

	if (_randomValues.isEmpty()) {
		return;
	}

	if (_randomValues.size() == 1) {
		const auto &value = _randomValues.at(0);
		var.SetValue(value);
		_lastRandomValue = value;
	}

	std::uniform_int_distribution<int> dis(0, _randomValues.size() - 1);
	StringVariable value = _randomValues.at(dis(gen));

	uint8_t iter = 0;
	for (; !_allowRepeatValues && iter < maxIter &&
	       _lastRandomValue.has_value() &&
	       std::string(value) == _lastRandomValue.value();
	     iter++) {
		value = _randomValues.at(dis(gen));
	}

	if (iter == maxIter) {
		blog(LOG_INFO,
		     "giving up picking non-repeat random value after %d tries",
		     maxIter);
	}

	var.SetValue(value);
	_lastRandomValue = value;
}

bool VariableActionMath::Perform(MacroActionVariable &,
				 MacroActionVariable::Action type,
				 Variable &var)
{
	switch (type) {
	case Action::MATH_EXPRESSION:
		HandleMathExpression(var);
		break;
	case Action::INCREMENT:
		modifyNumValue(var, _numValue.GetValue(), true);
		break;
	case Action::DECREMENT:
		modifyNumValue(var, _numValue.GetValue(), false);
		break;
	case Action::ROUND_TO_INT: {
		auto curValue = var.DoubleValue();
		if (!curValue.has_value()) {
			break;
		}
		var.SetValue(std::to_string(int(std::round(*curValue))));
		break;
	}
	case Action::RANDOM_NUMBER:
		GenerateRandomNumber(var);
		break;
	case Action::RANDOM_LIST_VALUE:
		PickRandomValue(var);
		break;
	default:
		break;
	}
	return true;
}

VariableActionMathEdit::VariableActionMathEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionMath>()
			  : nullptr),
	  _numValue(new VariableDoubleSpinBox(this)),
	  _mathExpression(new VariableLineEdit(this)),
	  _mathExpressionResult(new QLabel(this)),
	  _randomNumberStart(new VariableDoubleSpinBox(this)),
	  _randomNumberEnd(new VariableDoubleSpinBox(this)),
	  _generateInteger(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.generateInteger"),
		  this)),
	  _randomNumberLayout(new QVBoxLayout()),
	  _randomValues(new StringListEdit(this)),
	  _allowRepeatValues(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.type.allowRepeat"),
		  this)),
	  _randomValueLayout(new QVBoxLayout())
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	_randomNumberStart->setMinimum(-9999999999);
	_randomNumberStart->setMaximum(9999999999);
	_randomNumberEnd->setMinimum(-9999999999);
	_randomNumberEnd->setMaximum(9999999999);
	_randomValues->SetMaxStringSize(99999999);

	QWidget::connect(
		_numValue,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(NumValueChanged(const NumberVariable<double> &)));
	QWidget::connect(_mathExpression, SIGNAL(editingFinished()), this,
			 SLOT(MathExpressionChanged()));
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
	QWidget::connect(_randomValues,
			 SIGNAL(StringListChanged(const StringList &)), this,
			 SLOT(RandomValueListChanged(const StringList &)));
	QWidget::connect(_allowRepeatValues, SIGNAL(stateChanged(int)), this,
			 SLOT(AllowRepeatValuesChanged(int)));

	auto randomLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.layout.randomNumber"),
		randomLayout,
		{{"{{randomNumberStart}}", _randomNumberStart},
		 {"{{randomNumberEnd}}", _randomNumberEnd}});
	_randomNumberLayout->addLayout(randomLayout);
	_randomNumberLayout->addWidget(_generateInteger);

	_randomValueLayout->addWidget(_randomValues);
	_randomValueLayout->addWidget(_allowRepeatValues);

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_mathExpressionResult);
	layout->addLayout(_randomNumberLayout);
	layout->addLayout(_randomValueLayout);
	setLayout(layout);
}

VariableActionGroupEdit *
VariableActionMathEdit::Create(QWidget *parent,
			       std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionMathEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionMathEdit::GetInlineWidgets()
{
	return {
		{"{{numValue}}", _numValue},
		{"{{mathExpression}}", _mathExpression},
	};
}

void VariableActionMathEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_numValue->SetValue(_data->_numValue);
	_mathExpression->setText(_data->_mathExpression);
	_randomNumberStart->SetValue(_data->_randomNumberStart);
	_randomNumberEnd->SetValue(_data->_randomNumberEnd);
	_generateInteger->setChecked(_data->_generateInteger);
	_allowRepeatValues->setChecked(_data->_allowRepeatValues);
	_randomValues->SetStringList(_data->_randomValues);
	_loading = false;
}

void VariableActionMathEdit::SetWidgetVisibility(
	MacroActionVariable::Action action)
{
	const bool isRandomNumber = action == Action::RANDOM_NUMBER;
	const bool isRandomListValue = action == Action::RANDOM_LIST_VALUE;
	_mathExpressionResult->hide();
	SetLayoutVisible(_randomNumberLayout, isRandomNumber);
	SetLayoutVisible(_randomValueLayout, isRandomListValue);
	setVisible(isRandomNumber || isRandomListValue);
	UpdateSize();
}

void VariableActionMathEdit::NumValueChanged(const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_numValue = value;
}

void VariableActionMathEdit::MathExpressionChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_mathExpression = _mathExpression->text().toStdString();

	// In case of invalid expression display an error
	auto result = EvalMathExpression(_data->_mathExpression);
	auto hasError = std::holds_alternative<std::string>(result);
	if (hasError) {
		_mathExpressionResult->setText(
			QString::fromStdString(std::get<std::string>(result)));
	}
	_mathExpressionResult->setVisible(hasError);
	setVisible(hasError);
	UpdateSize();
}

void VariableActionMathEdit::RandomNumberStartChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_randomNumberStart = value;
}

void VariableActionMathEdit::RandomNumberEndChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_randomNumberEnd = value;
}

void VariableActionMathEdit::GenerateIntegerChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_generateInteger = value;
}

void VariableActionMathEdit::RandomValueListChanged(const StringList &values)
{
	GUARD_LOADING_AND_LOCK();
	_data->_randomValues = values;
}

void VariableActionMathEdit::AllowRepeatValuesChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_allowRepeatValues = value;
}

} // namespace advss
