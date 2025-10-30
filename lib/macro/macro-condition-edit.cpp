#include "macro-condition-edit.hpp"
#include "advanced-scene-switcher.hpp"
#include "macro-settings.hpp"
#include "macro.hpp"
#include "path-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "section.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

namespace advss {

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, condition] :
	     MacroConditionFactory::GetConditionTypes()) {
		QString entry(obs_module_text(condition._name.c_str()));
		if (list->findText(entry) == -1) {
			list->addItem(entry);
		} else {
			blog(LOG_WARNING,
			     "did not insert duplicate condition entry with name \"%s\"",
			     entry.toStdString().c_str());
		}
	}
	list->model()->sort(0);
}

static void populateDurationModifierTypes(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.more"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.equal"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.less"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.within"));
}

DurationModifierEdit::DurationModifierEdit(QWidget *parent)
{
	_condition = new QComboBox(parent);
	_duration = new DurationSelection(parent);
	_toggle = new QPushButton(parent);
	_toggle->setMaximumWidth(22);
	const auto path = QString::fromStdString(GetDataFilePath(
		"res/images/" + GetThemeTypeName() + "Time.svg"));
	_toggle->setIcon(QIcon(path));
	populateDurationModifierTypes(_condition);
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(_ModifierChanged(int)));
	QObject::connect(_duration, &DurationSelection::DurationChanged, this,
			 &DurationModifierEdit::DurationChanged);
	QWidget::connect(_toggle, SIGNAL(clicked()), this,
			 SLOT(ToggleClicked()));

	QHBoxLayout *layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(11);
	layout->addWidget(_toggle);
	layout->addWidget(_condition);
	layout->addWidget(_duration);
	setLayout(layout);
	Collapse(true);
}

void DurationModifierEdit::SetValue(DurationModifier &value)
{
	_duration->SetDuration(value.GetDuration());
	_condition->setCurrentIndex(static_cast<int>(value.GetType()));
	_duration->setVisible(value.GetType() != DurationModifier::Type::NONE);
}

void DurationModifierEdit::SetDuration(const Duration &d)
{
	_duration->SetDuration(d);
}

void DurationModifierEdit::_ModifierChanged(int value)
{
	auto m = static_cast<DurationModifier::Type>(value);
	Collapse(m == DurationModifier::Type::NONE);
	emit ModifierChanged(m);
}

void DurationModifierEdit::ToggleClicked()
{
	Collapse(false);
}

void DurationModifierEdit::Collapse(bool collapse)
{
	_toggle->setVisible(collapse);
	_duration->setVisible(!collapse);
	_condition->setVisible(!collapse);
}

MacroConditionEdit::MacroConditionEdit(
	QWidget *parent, std::shared_ptr<MacroCondition> *entryData,
	const std::string &id, bool isRootCondition)
	: MacroSegmentEdit(parent),
	  _logicSelection(new QComboBox()),
	  _conditionSelection(new FilterComboBox()),
	  _dur(new DurationModifierEdit()),
	  _entryData(entryData),
	  _isRoot(isRootCondition)
{
	QWidget::connect(_logicSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(LogicSelectionChanged(int)));
	QWidget::connect(_conditionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ConditionSelectionChanged(const QString &)));
	QWidget::connect(_dur, SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_dur, SIGNAL(ModifierChanged(DurationModifier::Type)),
			 this,
			 SLOT(DurationModifierChanged(DurationModifier::Type)));

	Logic::PopulateLogicTypeSelection(_logicSelection, isRootCondition);
	populateConditionSelection(_conditionSelection);

	_section->AddHeaderWidget(_logicSelection);
	_section->AddHeaderWidget(_conditionSelection);
	_section->AddHeaderWidget(_headerInfo);
	_section->AddHeaderWidget(_dur);

	QVBoxLayout *conditionLayout = new QVBoxLayout;
	conditionLayout->setContentsMargins({7, 7, 7, 7});
	conditionLayout->addWidget(_section);
	_contentLayout->addLayout(conditionLayout);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	mainLayout->addWidget(_frame);
	setLayout(mainLayout);

	SetupWidgets(true);
	_loading = false;
}

void MacroConditionEdit::LogicSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	const auto logic = static_cast<Logic::Type>(
		_logicSelection->itemData(idx).toInt());
	(*_entryData)->SetLogicType(logic);

	SetEnableAppearance(logic != Logic::Type::NONE);
}

bool MacroConditionEdit::IsRootNode() const
{
	return _isRoot;
}

void MacroConditionEdit::SetLogicSelection()
{
	const auto logic = (*_entryData)->GetLogicType();
	_logicSelection->setCurrentIndex(
		_logicSelection->findData(static_cast<int>(logic)));
	SetEnableAppearance(logic != Logic::Type::NONE);
}

void MacroConditionEdit::SetRootNode(bool root)
{
	_isRoot = root;
	const QSignalBlocker blocker(_logicSelection);
	_logicSelection->clear();
	Logic::PopulateLogicTypeSelection(_logicSelection, root);
	SetLogicSelection();
}

void MacroConditionEdit::SetupWidgets(bool basicSetup)
{
	if (_allWidgetsAreSetup) {
		return;
	}

	const auto id = (*_entryData)->GetId();
	_conditionSelection->setCurrentText(obs_module_text(
		MacroConditionFactory::GetConditionName(id).c_str()));

	HeaderInfoChanged(
		QString::fromStdString((*_entryData)->GetShortDesc()));
	SetLogicSelection();

	_dur->setVisible(MacroConditionFactory::UsesDurationModifier(id));
	auto modifier = (*_entryData)->GetDurationModifier();
	_dur->SetValue(modifier);

	if (basicSetup) {
		return;
	}

	auto widget =
		MacroConditionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget, (*_entryData)->GetCollapsed());
	SetFocusPolicyOfWidgets();

	_allWidgetsAreSetup = true;
}

void MacroConditionEdit::SetEntryData(std::shared_ptr<MacroCondition> *data)
{
	_entryData = data;
}

void MacroConditionEdit::ConditionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::string id = MacroConditionFactory::GetIdByName(text);
	if (id.empty()) {
		return;
	}

	auto temp = DurationModifier();
	_dur->SetValue(temp);
	HeaderInfoChanged("");
	auto idx = _entryData->get()->GetIndex();
	auto macro = _entryData->get()->GetMacro();
	{
		auto lock = LockContext();
		auto logic = (*_entryData)->GetLogicType();
		_entryData->reset();
		*_entryData = MacroConditionFactory::Create(id, macro);
		(*_entryData)->SetIndex(idx);
		(*_entryData)->SetLogicType(logic);
		(*_entryData)->PostLoad();
		RunAndClearPostLoadSteps();
	}
	auto widget =
		MacroConditionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget);
	_dur->setVisible(MacroConditionFactory::UsesDurationModifier(id));
	SetFocusPolicyOfWidgets();
}

void MacroConditionEdit::DurationChanged(const Duration &seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	(*_entryData)->SetDuration(seconds);
}

void MacroConditionEdit::DurationModifierChanged(DurationModifier::Type m)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	(*_entryData)->SetDurationModifier(m);
}

std::shared_ptr<MacroSegment> MacroConditionEdit::Data() const
{
	return *_entryData;
}

} // namespace advss
