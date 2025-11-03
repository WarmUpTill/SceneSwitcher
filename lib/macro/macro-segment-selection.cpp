#include "macro-segment-selection.hpp"
#include "advanced-scene-switcher.hpp"
#include "macro-action-factory.hpp"
#include "macro-condition-factory.hpp"
#include "macro-helpers.hpp"
#include "macro-segment.hpp"
#include "macro-signals.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

#include <QHBoxLayout>

namespace advss {

MacroSegmentSelection::MacroSegmentSelection(QWidget *parent, Type type,
					     bool allowVariables)
	: QWidget(parent),
	  _index(new VariableSpinBox()),
	  _description(new QLabel()),
	  _type(type)
{
	_index->setMinimum(0);
	_index->setMaximum(99);
	_index->setSpecialValueText("-");
	if (!allowVariables) {
		_index->DisableVariableSelection();
	}
	SetupDescription();

	QWidget::connect(
		_index,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(IndexChanged(const NumberVariable<int> &)));
	QWidget::connect(MacroSignalManager::Instance(),
			 SIGNAL(SegmentOrderChanged()), this,
			 SLOT(MacroSegmentOrderChanged()));

	auto layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_index);
	layout->addWidget(_description);
	setLayout(layout);
}

void MacroSegmentSelection::SetMacro(const std::shared_ptr<Macro> &macro)
{
	_macro = macro.get();
	SetupDescription();
}

void MacroSegmentSelection::SetMacro(Macro *macro)
{
	_macro = macro;
	SetupDescription();
}

void MacroSegmentSelection::SetValue(const IntVariable &value)
{
	_index->SetValue(value);
	SetupDescription();
}

void MacroSegmentSelection::SetType(const Type &value)
{
	_type = value;
	SetupDescription();
}

void MacroSegmentSelection::MacroSegmentOrderChanged()
{
	SetupDescription();
}

static QString GetMacroSegmentDescription(Macro *macro, int idx,
					  MacroSegmentSelection::Type type)
{
	if (!macro) {
		return "";
	}

	MacroSegment *segment;

	switch (type) {
	case MacroSegmentSelection::Type::CONDITION:
		if (!IsValidConditionIndex(macro, idx)) {
			return "";
		}
		segment = GetMacroConditions(macro)->at(idx).get();
		break;
	case MacroSegmentSelection::Type::ACTION:
		if (!IsValidActionIndex(macro, idx)) {
			return "";
		}
		segment = GetMacroActions(macro)->at(idx).get();
		break;
	case MacroSegmentSelection::Type::ELSE_ACTION:
		if (!IsValidElseActionIndex(macro, idx)) {
			return "";
		}
		segment = GetMacroElseActions(macro)->at(idx).get();
		break;
	default:
		return "";
	}

	const auto idToName = type == MacroSegmentSelection::Type::CONDITION
				      ? MacroConditionFactory::GetConditionName
				      : MacroActionFactory::GetActionName;
	const QString typeStr =
		obs_module_text(idToName(segment->GetId()).c_str());
	const QString description =
		QString::fromStdString(segment->GetShortDesc());

	QString result = typeStr;
	if (!description.isEmpty()) {
		result += ": " + description;
	}
	return result;
}

void MacroSegmentSelection::SetupDescription() const
{
	if (!_macro) {
		_description->setText("");
		_description->hide();
		return;
	}

	auto index = _index->Value();
	if (index.GetType() == IntVariable::Type::VARIABLE || index == 0) {
		_description->setText("");
		_description->hide();
		return;
	}

	bool validIndex = false;
	switch (_type) {
	case Type::CONDITION:
		validIndex = IsValidConditionIndex(_macro, index - 1);
		break;
	case Type::ACTION:
		validIndex = IsValidActionIndex(_macro, index - 1);
		break;
	case Type::ELSE_ACTION:
		validIndex = IsValidElseActionIndex(_macro, index - 1);
		break;
	default:
		break;
	}

	QString description;
	if (validIndex) {
		description =
			GetMacroSegmentDescription(_macro, index - 1, _type);
	} else {
		description = obs_module_text(
			"AdvSceneSwitcher.macroSegmentSelection.invalid");
	}

	if (!description.isEmpty()) {
		description = "(" + description + ")";
		_description->setText(description);
		_description->show();
	} else {
		_description->setText("");
		_description->hide();
	}
}

void MacroSegmentSelection::IndexChanged(const IntVariable &value)
{
	SetupDescription();
	MarkSelectedSegment();
	emit(SelectionChanged(value));
}

void MacroSegmentSelection::MarkSelectedSegment()
{
	if (!HighlightUIElementsEnabled()) {
		return;
	}

	if (!_macro || !AdvSceneSwitcher::window ||
	    _macro != AdvSceneSwitcher::window->GetSelectedMacro().get()) {
		return;
	}

	auto index = _index->Value();
	if (index.GetType() == IntVariable::Type::VARIABLE) {
		return;
	}

	bool validIndex = false;
	switch (_type) {
	case Type::CONDITION:
		validIndex = IsValidConditionIndex(_macro, index - 1);
		break;
	case Type::ACTION:
		validIndex = IsValidActionIndex(_macro, index - 1);
		break;
	case Type::ELSE_ACTION:
		validIndex = IsValidElseActionIndex(_macro, index - 1);
		break;
	default:
		break;
	}

	if (!validIndex) {
		return;
	}

	if (_type == Type::CONDITION) {
		AdvSceneSwitcher::window->HighlightCondition(
			index - 1, QColor(Qt::lightGray));
	} else {
		AdvSceneSwitcher::window->HighlightAction(
			index - 1, QColor(Qt::lightGray));
	}

	HighlightWidget(this, QColor(Qt::lightGray), QColor(0, 0, 0, 0), true);
}

} // namespace advss
