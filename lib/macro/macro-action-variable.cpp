#include "macro-action-variable.hpp"
#include "layout-helpers.hpp"
#include "macro-action-variable-group.hpp"

namespace advss {

const std::string MacroActionVariable::id = "variable";

bool MacroActionVariable::_registered = MacroActionFactory::Register(
	MacroActionVariable::id,
	{MacroActionVariable::Create, MacroActionVariableEdit::Create,
	 "AdvSceneSwitcher.action.variable"});

MacroActionVariable::MacroActionVariable(Macro *m) : MacroAction(m)
{
	for (const auto &group : GetVariableActionGroups()) {
		_groups.emplace_back(group.createData());
	}
}

MacroActionVariable::MacroActionVariable(const MacroActionVariable &other)
	: MacroAction(other),
	  _action(other._action),
	  _variable(other._variable),
	  _variable2(other._variable2),
	  _strValue(other._strValue)
{
	for (const auto &group : other._groups) {
		_groups.emplace_back(group->Copy());
	}
}

MacroActionVariable::~MacroActionVariable() = default;

std::vector<TempVariableRef> MacroActionVariable::GetTempVarRefs() const
{
	auto refs = MacroSegment::GetTempVarRefs();
	for (const auto &group : _groups) {
		auto groupRefs = group->GetTempVarRefs();
		refs.insert(refs.end(), groupRefs.begin(), groupRefs.end());
	}
	return refs;
}

bool MacroActionVariable::PerformAction()
{
	auto var = _variable.lock();
	if (!var) {
		return true;
	}

	const int idx = GetVariableActionGroupIndex(_action);
	if (idx < 0 || idx >= (int)_groups.size()) {
		return true;
	}

	return _groups[idx]->Perform(*this, _action, *var);
}

bool MacroActionVariable::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_variable.Save(obj, "variableName");
	_variable2.Save(obj, "variable2Name");
	_strValue.Save(obj, "strValue");
	obs_data_set_int(obj, "condition", static_cast<int>(_action));
	for (const auto &group : _groups) {
		group->Save(obj, *this);
	}
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroActionVariable::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_variable.Load(obj, "variableName");
	_variable2.Load(obj, "variable2Name");
	_strValue.Load(obj, "strValue");
	_action = static_cast<Action>(obs_data_get_int(obj, "condition"));
	for (const auto &group : _groups) {
		group->Load(obj, *this);
	}
	return true;
}

bool MacroActionVariable::PostLoad()
{
	for (const auto &group : _groups) {
		group->PostLoad(*this);
	}
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

void MacroActionVariable::ResolveVariablesToFixedValues()
{
	_strValue.ResolveVariables();
	for (const auto &group : _groups) {
		group->ResolveVariables();
	}
}

static void populateActionSelection(QComboBox *list)
{
	const auto &groups = GetVariableActionGroups();
	for (size_t idx = 0; idx < groups.size(); idx++) {
		if (idx > 0) {
			list->insertSeparator(list->count());
		}
		for (const auto &option : groups[idx].options) {
			list->addItem(obs_module_text(option.label),
				      static_cast<int>(option.action));
		}
	}
}

MacroActionVariableEdit::MacroActionVariableEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: QWidget(parent),
	  _variables(new VariableSelection(this)),
	  _variables2(new VariableSelection(this)),
	  _actions(new FilterComboBox(this)),
	  _strValue(new VariableTextEdit(this, 5, 1, 1)),
	  _entryLayout(new QHBoxLayout())
{
	populateActionSelection(_actions);

	QWidget::connect(_variables, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(VariableChanged(const QString &)));
	QWidget::connect(_variables2, SIGNAL(SelectionChanged(const QString &)),
			 this, SLOT(Variable2Changed(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_strValue, SIGNAL(textChanged()), this,
			 SLOT(StrValueChanged()));

	auto layout = new QVBoxLayout;
	layout->addLayout(_entryLayout);
	for (const auto &group : GetVariableActionGroups()) {
		auto groupEdit = group.createEdit(this, entryData);
		_groupEdits.push_back(groupEdit);
		layout->addWidget(groupEdit);
	}
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
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
	for (const auto &groupEdit : _groupEdits) {
		groupEdit->UpdateEntryData();
	}

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
	SetWidgetVisibility();
}

void MacroActionVariableEdit::StrValueChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_strValue = _strValue->toPlainText().toStdString();
	adjustSize();
	updateGeometry();
}

std::unordered_map<std::string, QWidget *>
MacroActionVariableEdit::GetInlineWidgets() const
{
	std::unordered_map<std::string, QWidget *> widgets = {
		{"{{variables}}", _variables},
		{"{{variables2}}", _variables2},
		{"{{actions}}", _actions},
		{"{{strValue}}", _strValue},
	};
	for (const auto &groupEdit : _groupEdits) {
		widgets.merge(groupEdit->GetInlineWidgets());
	}
	return widgets;
}

void MacroActionVariableEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	const auto option = GetVariableActionOption(_entryData->_action);
	if (!option) {
		return;
	}

	const auto widgets = GetInlineWidgets();
	for (const auto &[_, widget] : widgets) {
		_entryLayout->removeWidget(widget);
	}
	ClearLayout(_entryLayout);

	const std::string layoutString = obs_module_text(option->layout);
	PlaceWidgets(layoutString, _entryLayout, widgets);

	for (const auto &[placeholder, widget] : widgets) {
		widget->setVisible(layoutString.find(placeholder) !=
				   std::string::npos);
	}

	if (option->expandInlineRow) {
		RemoveStretchIfPresent(_entryLayout);
	} else {
		AddStretchIfNecessary(_entryLayout);
	}

	for (const auto &groupEdit : _groupEdits) {
		groupEdit->SetWidgetVisibility(_entryData->_action);
	}

	adjustSize();
	updateGeometry();
}

} // namespace advss
