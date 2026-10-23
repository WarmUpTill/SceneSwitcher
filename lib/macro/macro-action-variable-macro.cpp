#include "macro-action-variable-macro.hpp"
#include "macro-condition-edit.hpp"
#include "macro-signals.hpp"
#include "macro.hpp"

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{30,
	 {
		 {Action::SET_TO_TEMPVAR,
		  "AdvSceneSwitcher.action.variable.type.setToTempvar",
		  "AdvSceneSwitcher.action.variable.layout.setToTempvar"},
		 {Action::SET_CONDITION_VALUE,
		  "AdvSceneSwitcher.action.variable.type.setConditionValue",
		  "AdvSceneSwitcher.action.variable.layout.segmentValue"},
		 {Action::SET_ACTION_VALUE,
		  "AdvSceneSwitcher.action.variable.type.setActionValue",
		  "AdvSceneSwitcher.action.variable.layout.segmentValue"},
	 },
	 []() { return std::make_unique<VariableActionMacro>(); },
	 VariableActionMacroEdit::Create});

static bool isSegmentValueAction(MacroActionVariable::Action action)
{
	return action == Action::SET_CONDITION_VALUE ||
	       action == Action::SET_ACTION_VALUE;
}

VariableActionMacro::~VariableActionMacro()
{
	DecrementCurrentSegmentVariableRef();
}

std::unique_ptr<VariableActionGroup> VariableActionMacro::Copy() const
{
	return std::make_unique<VariableActionMacro>(*this);
}

void VariableActionMacro::Save(obs_data_t *obj,
			       const MacroActionVariable &action) const
{
	obs_data_set_int(obj, "segmentIdx", GetSegmentIndexValue(action));
	_tempVar.Save(obj, action.GetMacro());
}

void VariableActionMacro::Load(obs_data_t *obj, MacroActionVariable &action)
{
	_segmentIdxLoadValue = obs_data_get_int(obj, "segmentIdx");
	_tempVar.Load(obj, action.GetMacro());
}

void VariableActionMacro::PostLoad(MacroActionVariable &action)
{
	SetSegmentIndexValue(action, _segmentIdxLoadValue);
}

std::vector<TempVariableRef> VariableActionMacro::GetTempVarRefs() const
{
	if (_tempVar.HasValidID()) {
		return {_tempVar};
	}
	return {};
}

bool VariableActionMacro::Perform(MacroActionVariable &action,
				  MacroActionVariable::Action type,
				  Variable &var)
{
	switch (type) {
	case Action::SET_CONDITION_VALUE:
	case Action::SET_ACTION_VALUE: {
		auto m = action.GetMacro();
		if (!m) {
			return true;
		}
		if (GetSegmentIndexValue(action) == -1) {
			return true;
		}
		auto segment = _macroSegment.lock();
		if (!segment) {
			return true;
		}
		var.SetValue(segment->GetVariableValue());
		return true;
	}
	case Action::SET_TO_TEMPVAR: {
		auto tempVar = _tempVar.GetTempVariable(action.GetMacro());
		if (!tempVar) {
			return true;
		}
		auto value = tempVar->Value();
		if (!value) {
			return true;
		}
		var.SetValue(*value);
		return true;
	}
	default:
		return true;
	}
}

void VariableActionMacro::SetSegmentIndexValue(
	const MacroActionVariable &action, int value)
{
	DecrementCurrentSegmentVariableRef();

	auto m = action.GetMacro();
	if (!m) {
		_macroSegment.reset();
		return;
	}

	if (value < 0) {
		_macroSegment.reset();
		return;
	}

	std::shared_ptr<MacroSegment> segment;
	if (action._action == Action::SET_CONDITION_VALUE) {
		if (value < (int)m->Conditions().size()) {
			segment = m->Conditions().at(value);
		}
	} else if (action._action == Action::SET_ACTION_VALUE) {
		if (value < (int)m->Actions().size()) {
			segment = m->Actions().at(value);
		}
	}

	_macroSegment = segment;
	if (segment) {
		IncrementVariableRef(segment.get());
	}
}

int VariableActionMacro::GetSegmentIndexValue(
	const MacroActionVariable &action) const
{
	auto m = action.GetMacro();
	if (!m) {
		return -1;
	}

	auto segment = _macroSegment.lock();
	if (!segment) {
		return -1;
	}

	if (action._action == Action::SET_CONDITION_VALUE) {
		auto it = std::find(m->Conditions().begin(),
				    m->Conditions().end(), segment);
		if (it != m->Conditions().end()) {
			return std::distance(m->Conditions().begin(), it);
		}
		return -1;
	} else if (action._action == Action::SET_ACTION_VALUE) {
		auto it = std::find(m->Actions().begin(), m->Actions().end(),
				    segment);
		if (it != m->Actions().end()) {
			return std::distance(m->Actions().begin(), it);
		}
		return -1;
	}

	return -1;
}

void VariableActionMacro::DecrementCurrentSegmentVariableRef()
{
	auto segment = _macroSegment.lock();
	if (!segment) {
		return;
	}

	DecrementVariableRef(segment.get());
}

VariableActionMacroEdit::VariableActionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionMacro>()
			  : nullptr),
	  _segmentIdx(new MacroSegmentSelection(
		  this, MacroSegmentSelection::Type::CONDITION, false)),
	  _segmentValueStatus(new QLabel(this)),
	  _segmentValue(new ResizingPlainTextEdit(this, 10, 1, 1)),
	  _tempVars(new TempVariableSelection(this)),
	  _tempVarsHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.type.setToTempvar.help"),
		  this))
{
	_segmentValue->setReadOnly(true);

	QWidget::connect(_segmentIdx,
			 SIGNAL(SelectionChanged(const IntVariable &)), this,
			 SLOT(SegmentIndexChanged(const IntVariable &)));
	QWidget::connect(MacroSignalManager::Instance(),
			 SIGNAL(SegmentOrderChanged()), this,
			 SLOT(MacroSegmentOrderChanged()));
	QWidget::connect(_tempVars,
			 SIGNAL(SelectionChanged(const TempVariableRef &)),
			 this, SLOT(SelectionChanged(const TempVariableRef &)));

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_segmentValueStatus);
	layout->addWidget(_segmentValue);
	setLayout(layout);

	connect(&_timer, SIGNAL(timeout()), this,
		SLOT(UpdateSegmentVariableValue()));
	_timer.start(1500);
}

VariableActionGroupEdit *
VariableActionMacroEdit::Create(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionMacroEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionMacroEdit::GetInlineWidgets()
{
	return {
		{"{{segmentIndex}}", _segmentIdx},
		{"{{tempVars}}", _tempVars},
		{"{{tempVarsHelp}}", _tempVarsHelp},
	};
}

void VariableActionMacroEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_segmentIdx->SetValue(_data->GetSegmentIndexValue(*_entryData) + 1);
	_segmentIdx->SetMacro(_entryData->GetMacro());
	_tempVars->SetVariable(_data->_tempVar);
	_loading = false;
	UpdateSegmentVariableValue();
}

void VariableActionMacroEdit::SetWidgetVisibility(
	MacroActionVariable::Action action)
{
	if (!_data) {
		return;
	}

	if (action == Action::SET_ACTION_VALUE) {
		_segmentIdx->SetType(MacroSegmentSelection::Type::ACTION);
	} else if (action == Action::SET_CONDITION_VALUE) {
		_segmentIdx->SetType(MacroSegmentSelection::Type::CONDITION);
	}

	_tempVarsHelp->setVisible(action == Action::SET_TO_TEMPVAR &&
				  !_data->_tempVar.HasValidID());
	setVisible(isSegmentValueAction(action));
	UpdateSize();
}

void VariableActionMacroEdit::SegmentIndexChanged(const IntVariable &val)
{
	GUARD_LOADING_AND_LOCK();
	_data->SetSegmentIndexValue(*_entryData, val - 1);
}

void VariableActionMacroEdit::SetSegmentValueError(const QString &text)
{
	_segmentValueStatus->setText(text);
	_segmentValue->setPlainText("");
	_segmentValue->hide();
	UpdateSize();
}

void VariableActionMacroEdit::UpdateSegmentVariableValue()
{
	if (!_data || !isSegmentValueAction(_entryData->_action)) {
		return;
	}

	auto m = _entryData->GetMacro();
	if (!m) {
		return;
	}

	int index = _data->GetSegmentIndexValue(*_entryData);
	if (index < 0) {
		SetSegmentValueError(obs_module_text(
			"AdvSceneSwitcher.action.variable.invalidSelection"));
		const QSignalBlocker b(_segmentIdx);
		_segmentIdx->SetValue(0);
		return;
	}

	std::shared_ptr<MacroSegment> segment;
	if (_entryData->_action == Action::SET_ACTION_VALUE) {
		const auto &actions = m->Actions();
		if (index < (int)actions.size()) {
			segment = actions.at(index);
		}
	} else if (_entryData->_action == Action::SET_CONDITION_VALUE) {
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

		if (_entryData->_action == Action::SET_ACTION_VALUE) {
			type = MacroActionFactory::GetActionName(
				segment->GetId());
			fmt = QString(obs_module_text(
				"AdvSceneSwitcher.action.variable.actionNoVariableSupport"));
		} else if (_entryData->_action == Action::SET_CONDITION_VALUE) {
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

	UpdateSize();
}

void VariableActionMacroEdit::MacroSegmentOrderChanged()
{
	if (!_data) {
		return;
	}

	const QSignalBlocker b(_segmentIdx);
	_segmentIdx->SetValue(_data->GetSegmentIndexValue(*_entryData) + 1);
}

void VariableActionMacroEdit::SelectionChanged(const TempVariableRef &var)
{
	GUARD_LOADING_AND_LOCK();
	_data->_tempVar = var;
	IncrementTempVarInUseGeneration();
	SetWidgetVisibility(_entryData->_action);
}

} // namespace advss
