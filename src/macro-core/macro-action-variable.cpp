#include "macro-action-variable.hpp"
#include "advanced-scene-switcher.hpp"
#include "macro-condition-edit.hpp"
#include "utility.hpp"

const std::string MacroActionVariable::id = "variable";

bool MacroActionVariable::_registered = MacroActionFactory::Register(
	MacroActionVariable::id,
	{MacroActionVariable::Create, MacroActionVariableEdit::Create,
	 "AdvSceneSwitcher.action.variable"});

static std::map<MacroActionVariable::Type, std::string> actionTypes = {
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
};

static void apppend(Variable &var, const std::string &value)
{
	auto currentValue = var.Value();
	var.SetValue(currentValue + value);
}

static void modifyNumValue(Variable &var, double val, const bool increment)
{
	double current;
	if (!var.DoubleValue(current)) {
		return;
	}
	if (increment) {
		var.SetValue(current + val);
	} else {
		var.SetValue(current - val);
	}
}

MacroActionVariable::~MacroActionVariable()
{
	DecrementCurrentSegmentVariableRef();
}

bool MacroActionVariable::PerformAction()
{
	auto var = GetVariableByName(_variableName);
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
		auto var2 = GetVariableByName(_variable2Name);
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
		double curValue;
		if (!var->DoubleValue(curValue)) {
			return true;
		}
		var->SetValue(std::to_string(int(std::round(curValue))));
		return true;
	}
	case Type::SUBSTRING: {
		auto curValue = var->Value();
		try {
			if (_subStringSize == 0) {
				var->SetValue(curValue.substr(_subStringStart));
				return true;
			}
			var->SetValue(curValue.substr(_subStringStart,
						      _subStringSize));
		} catch (const std::out_of_range &) {
			vblog(LOG_WARNING,
			      "invalid start index \"%d\" selected for substring of \"%s\" of variable \"%s\"",
			      _subStringStart, curValue.c_str(),
			      var->Name().c_str());
		}
		return true;
	}
	}

	return true;
}

bool MacroActionVariable::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "variableName", _variableName.c_str());
	obs_data_set_string(obj, "variable2Name", _variable2Name.c_str());
	obs_data_set_string(obj, "strValue", _strValue.c_str());
	obs_data_set_double(obj, "numValue", _numValue);
	obs_data_set_int(obj, "condition", static_cast<int>(_type));
	obs_data_set_int(obj, "segmentIdx", GetSegmentIndexValue());
	obs_data_set_int(obj, "subStringStart", _subStringStart);
	obs_data_set_int(obj, "subStringSize", _subStringSize);
	return true;
}

bool MacroActionVariable::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_variableName = obs_data_get_string(obj, "variableName");
	_variable2Name = obs_data_get_string(obj, "variable2Name");
	_strValue = obs_data_get_string(obj, "strValue");
	_numValue = obs_data_get_double(obj, "numValue");
	_type = static_cast<Type>(obs_data_get_int(obj, "condition"));
	_segmentIdxLoadValue = obs_data_get_int(obj, "segmentIdx");
	_subStringStart = obs_data_get_int(obj, "subStringStart");
	_subStringSize = obs_data_get_int(obj, "subStringSize");
	return true;
}

bool MacroActionVariable::PostLoad()
{
	SetSegmentIndexValue(_segmentIdxLoadValue);
	return true;
}

std::string MacroActionVariable::GetShortDesc() const
{
	return _variableName;
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
		segment->IncrementVariableRef();
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

void MacroActionVariable::DecrementCurrentSegmentVariableRef()
{
	auto segment = _macroSegment.lock();
	if (!segment) {
		return;
	}

	segment->DecrementVariableRef();
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionVariableEdit::MacroActionVariableEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: QWidget(parent),
	  _variables(new VariableSelection(this)),
	  _variables2(new VariableSelection(this)),
	  _actions(new QComboBox()),
	  _strValue(new ResizingPlainTextEdit(this, 5, 1, 1)),
	  _numValue(new QDoubleSpinBox()),
	  _segmentIdx(new QSpinBox()),
	  _segmentValueStatus(new QLabel()),
	  _segmentValue(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _substringLayout(new QHBoxLayout()),
	  _subStringStart(new QSpinBox()),
	  _subStringSize(new QSpinBox())
{
	_numValue->setMinimum(-9999999999);
	_numValue->setMaximum(9999999999);
	_segmentIdx->setMinimum(0);
	_segmentIdx->setMaximum(99);
	_segmentIdx->setSpecialValueText("-");
	_segmentValue->setReadOnly(true);
	_subStringStart->setMinimum(1);
	_subStringStart->setMaximum(99999);
	_subStringStart->setSpecialValueText(obs_module_text(
		"AdvSceneSwitcher.action.variable.subString.begin"));
	_subStringSize->setMinimum(0);
	_subStringSize->setMaximum(99999);
	_subStringSize->setSpecialValueText(obs_module_text(
		"AdvSceneSwitcher.action.variable.subString.all"));
	populateTypeSelection(_actions);

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
	QWidget::connect(_segmentIdx, SIGNAL(valueChanged(int)), this,
			 SLOT(SegmentIndexChanged(int)));
	QWidget::connect(window(), SIGNAL(MacroSegmentOrderChanged()), this,
			 SLOT(MacroSegmentOrderChanged()));
	QWidget::connect(_subStringStart, SIGNAL(valueChanged(int)), this,
			 SLOT(SubStringStartChanged(int)));
	QWidget::connect(_subStringSize, SIGNAL(valueChanged(int)), this,
			 SLOT(SubStringSizeChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{variables}}", _variables},
		{"{{variables2}}", _variables2},
		{"{{actions}}", _actions},
		{"{{strValue}}", _strValue},
		{"{{numValue}}", _numValue},
		{"{{segmentIndex}}", _segmentIdx},
		{"{{subStringStart}}", _subStringStart},
		{"{{subStringSize}}", _subStringSize},
	};
	auto entryLayout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.variable.entry"),
		     entryLayout, widgetPlaceholders);

	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.variable.entry.substring"),
		_substringLayout, widgetPlaceholders);

	auto layout = new QVBoxLayout;
	layout->addLayout(entryLayout);
	layout->addLayout(_substringLayout);
	layout->addWidget(_segmentValueStatus);
	layout->addWidget(_segmentValue);
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

	_variables->SetVariable(_entryData->_variableName);
	_variables2->SetVariable(_entryData->_variable2Name);
	_actions->setCurrentIndex(static_cast<int>(_entryData->_type));
	_strValue->setPlainText(QString::fromStdString(_entryData->_strValue));
	_numValue->setValue(_entryData->_numValue);
	_segmentIdx->setValue(_entryData->GetSegmentIndexValue() + 1);
	_subStringStart->setValue(_entryData->_subStringStart + 1);
	_subStringSize->setValue(_entryData->_subStringSize);
	SetWidgetVisibility();
}

void MacroActionVariableEdit::VariableChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_variableName = text.toStdString();
}

void MacroActionVariableEdit::Variable2Changed(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_variable2Name = text.toStdString();
}

void MacroActionVariableEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<MacroActionVariable::Type>(value);
	SetWidgetVisibility();

	if (_entryData->_type == MacroActionVariable::Type::SET_ACTION_VALUE ||
	    _entryData->_type ==
		    MacroActionVariable::Type::SET_CONDITION_VALUE) {
		MarkSelectedSegment();
	}
}

void MacroActionVariableEdit::StrValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
}

void MacroActionVariableEdit::NumValueChanged(double val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_numValue = val;
}

void MacroActionVariableEdit::SegmentIndexChanged(int val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->SetSegmentIndexValue(val - 1);
	MarkSelectedSegment();
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
		_segmentIdx->setValue(index);
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

	if (!segment->SupportsVariableValue()) {
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
	_segmentValue->setPlainText(
		QString::fromStdString(segment->GetVariableValue()));

	adjustSize();
	updateGeometry();
}

void MacroActionVariableEdit::MacroSegmentOrderChanged()
{
	const QSignalBlocker b(_segmentIdx);
	_segmentIdx->setValue(_entryData->GetSegmentIndexValue() + 1);
}

void MacroActionVariableEdit::SubStringStartChanged(int val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_subStringStart = val - 1;
}

void MacroActionVariableEdit::SubStringSizeChanged(int val)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_subStringSize = val;
}

void MacroActionVariableEdit::MarkSelectedSegment()
{
	if (switcher->disableHints) {
		return;
	}

	auto m = _entryData->GetMacro();
	if (!m) {
		return;
	}

	int index = _entryData->GetSegmentIndexValue();
	if (index < 0) {
		return;
	}

	if (_entryData->_type == MacroActionVariable::Type::SET_ACTION_VALUE) {
		const auto &actions = m->Actions();
		if (index >= (int)actions.size()) {
			return;
		}
		AdvSceneSwitcher::window->HighlightAction(
			index, QColor(Qt::lightGray));
	} else {
		const auto &conditions = m->Conditions();
		if (index >= (int)conditions.size()) {
			return;
		}
		AdvSceneSwitcher::window->HighlightCondition(
			index, QColor(Qt::lightGray));
	}

	PulseWidget(_segmentIdx, QColor(Qt::lightGray), QColor(0, 0, 0, 0),
		    true);
}

void MacroActionVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_variables2->setVisible(_entryData->_type ==
				MacroActionVariable::Type::APPEND_VAR);
	_strValue->setVisible(
		_entryData->_type ==
			MacroActionVariable::Type::SET_FIXED_VALUE ||
		_entryData->_type == MacroActionVariable::Type::APPEND);
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
	setLayoutVisible(_substringLayout,
			 _entryData->_type ==
				 MacroActionVariable::Type::SUBSTRING);

	adjustSize();
	updateGeometry();
}
