#include "headers/advanced-scene-switcher.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene.hpp"
#include "headers/section.hpp"
#include "headers/macro-controls.hpp"
#include "headers/utility.hpp"

std::map<std::string, MacroConditionInfo> MacroConditionFactory::_methods;

bool MacroConditionFactory::Register(const std::string &id,
				     MacroConditionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroCondition>
MacroConditionFactory::Create(const std::string &id, Macro *m)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._createFunc(m);
	}
	return nullptr;
}

QWidget *
MacroConditionFactory::CreateWidget(const std::string &id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._createWidgetFunc(parent, cond);
	}
	return nullptr;
}

std::string MacroConditionFactory::GetConditionName(const std::string &id)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._name;
	}
	return "unknown condition";
}

std::string MacroConditionFactory::GetIdByName(const QString &name)
{
	for (auto it : _methods) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool MacroConditionFactory::UsesDurationConstraint(const std::string &id)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._useDurationConstraint;
	}
	return false;
}

static inline void populateLogicSelection(QComboBox *list, bool root = false)
{
	if (root) {
		for (auto entry : MacroCondition::logicTypes) {
			if (static_cast<int>(entry.first) < logic_root_offset) {
				list->addItem(obs_module_text(
					entry.second._name.c_str()));
			}
		}
	} else {
		for (auto entry : MacroCondition::logicTypes) {
			if (static_cast<int>(entry.first) >=
			    logic_root_offset) {
				list->addItem(obs_module_text(
					entry.second._name.c_str()));
			}
		}
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : MacroConditionFactory::GetConditionTypes()) {
		list->addItem(obs_module_text(entry.second._name.c_str()));
	}
	list->model()->sort(0);
}

MacroConditionEdit::MacroConditionEdit(
	QWidget *parent, std::shared_ptr<MacroCondition> *entryData,
	const std::string &id, bool root)
	: MacroSegmentEdit(switcher->useVerticalMacroControls, parent),
	  _entryData(entryData),
	  _isRoot(root)
{
	_logicSelection = new QComboBox();
	_conditionSelection = new QComboBox();
	_dur = new DurationConstraintEdit();

	QWidget::connect(_logicSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(LogicSelectionChanged(int)));
	QWidget::connect(_conditionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ConditionSelectionChanged(const QString &)));
	QWidget::connect(_dur, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_dur, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(_dur, SIGNAL(ConditionChanged(DurationCondition)),
			 this,
			 SLOT(DurationConditionChanged(DurationCondition)));

	populateLogicSelection(_logicSelection, root);
	populateConditionSelection(_conditionSelection);

	_section->AddHeaderWidget(_logicSelection);
	_section->AddHeaderWidget(_conditionSelection);
	_section->AddHeaderWidget(_headerInfo);
	_section->AddHeaderWidget(_dur);

	QVBoxLayout *conditionLayout = new QVBoxLayout;
	conditionLayout->addWidget(_frame);
	_highLightFrameLayout->addWidget(_section);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	if (switcher->useVerticalMacroControls) {
		mainLayout->addWidget(_controls);
	} else {
		conditionLayout->addWidget(_controls);
	}
	mainLayout->addLayout(conditionLayout);
	setLayout(mainLayout);

	UpdateEntryData(id);
	_loading = false;
}

void MacroConditionEdit::LogicSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	LogicType type;
	if (IsRootNode()) {
		type = static_cast<LogicType>(idx);
	} else {
		type = static_cast<LogicType>(idx + logic_root_offset);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetLogicType(type);
}

bool MacroConditionEdit::IsRootNode()
{
	return _isRoot;
}

void MacroConditionEdit::SetRootNode(bool root)
{
	const QSignalBlocker blocker(_logicSelection);
	_logicSelection->clear();
	populateLogicSelection(_logicSelection, root);
}

void MacroConditionEdit::UpdateEntryData(const std::string &id)
{
	_conditionSelection->setCurrentText(obs_module_text(
		MacroConditionFactory::GetConditionName(id).c_str()));
	auto widget =
		MacroConditionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	HeaderInfoChanged(
		QString::fromStdString((*_entryData)->GetShortDesc()));
	auto logic = (*_entryData)->GetLogicType();
	if (IsRootNode()) {
		_logicSelection->setCurrentIndex(static_cast<int>(logic));
	} else {
		_logicSelection->setCurrentIndex(static_cast<int>(logic) -
						 logic_root_offset);
	}
	_section->SetContent(widget, (*_entryData)->GetCollapsed());

	_dur->setVisible(MacroConditionFactory::UsesDurationConstraint(id));
	auto constraint = (*_entryData)->GetDurationConstraint();
	_dur->SetValue(constraint);
	SetFocusPolicyOfWidgets();
}

void MacroConditionEdit::ConditionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto idx = _entryData->get()->GetIndex();
	auto macro = _entryData->get()->GetMacro();
	std::string id = MacroConditionFactory::GetIdByName(text);

	auto temp = DurationConstraint();
	_dur->SetValue(temp);
	HeaderInfoChanged("");

	std::lock_guard<std::mutex> lock(switcher->m);
	auto logic = (*_entryData)->GetLogicType();
	_entryData->reset();
	*_entryData = MacroConditionFactory::Create(id, macro);
	(*_entryData)->SetIndex(idx);
	(*_entryData)->SetLogicType(logic);
	auto widget =
		MacroConditionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget, false);
	_dur->setVisible(MacroConditionFactory::UsesDurationConstraint(id));
	SetFocusPolicyOfWidgets();
}

void MacroConditionEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetDuration(seconds);
}

void MacroConditionEdit::DurationConditionChanged(DurationCondition cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetDurationCondition(cond);
}

void MacroConditionEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	(*_entryData)->SetDurationUnit(unit);
}

MacroSegment *MacroConditionEdit::Data()
{
	return _entryData->get();
}

void AdvSceneSwitcher::AddMacroCondition(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > (int)macro->Conditions().size()) {
		return;
	}

	std::string id;
	if (idx - 1 >= 0) {
		id = macro->Conditions().at(idx - 1)->GetId();
	} else {
		MacroConditionScene temp(macro);
		id = temp.GetId();
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	bool root = idx == 0;
	auto cond = macro->Conditions().emplace(
		macro->Conditions().begin() + idx,
		MacroConditionFactory::Create(id, macro));
	if (idx - 1 >= 0) {
		auto data = obs_data_create();
		macro->Conditions().at(idx - 1)->Save(data);
		macro->Conditions().at(idx)->Load(data);
		obs_data_release(data);
	}
	auto logic = root ? LogicType::ROOT_NONE : LogicType::NONE;
	(*cond)->SetLogicType(logic);
	macro->UpdateConditionIndices();

	clearLayout(ui->macroEditConditionLayout, idx);
	PopulateMacroConditions(*macro, idx);
	HighlightCondition(idx);
}

void AdvSceneSwitcher::on_conditionAdd_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	AddMacroCondition((int)macro->Conditions().size());
	ui->macroEditConditionHelp->setVisible(false);
}

void AdvSceneSwitcher::RemoveMacroCondition(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Conditions().size()) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	macro->Conditions().erase(macro->Conditions().begin() + idx);
	macro->UpdateConditionIndices();

	if (idx == 0 && macro->Conditions().size() > 0) {
		auto newRoot = macro->Conditions().at(0);
		newRoot->SetLogicType(LogicType::ROOT_NONE);
	}

	clearLayout(ui->macroEditConditionLayout, idx);
	PopulateMacroConditions(*macro, idx);
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	RemoveMacroCondition((int)macro->Conditions().size() - 1);
}

void AdvSceneSwitcher::SwapConditions(Macro *m, int pos1, int pos2)
{
	if (pos1 == pos2) {
		return;
	}
	if (pos1 > pos2) {
		std::swap(pos1, pos2);
	}

	bool root = pos1 == 0;
	std::lock_guard<std::mutex> lock(switcher->m);
	iter_swap(m->Conditions().begin() + pos1,
		  m->Conditions().begin() + pos2);
	m->UpdateConditionIndices();

	auto c1 = m->Conditions().begin() + pos1;
	auto c2 = m->Conditions().begin() + pos2;
	if (root) {
		auto logic1 = (*c1)->GetLogicType();
		auto logic2 = (*c2)->GetLogicType();
		(*c1)->SetLogicType(logic2);
		(*c2)->SetLogicType(logic1);
	}

	auto item1 = ui->macroEditConditionLayout->takeAt(pos1);
	auto item2 = ui->macroEditConditionLayout->takeAt(pos2 - 1);
	deleteLayoutItem(item1);
	deleteLayoutItem(item2);
	auto widget1 =
		new MacroConditionEdit(this, &(*c1), (*c1)->GetId(), root);
	auto widget2 =
		new MacroConditionEdit(this, &(*c2), (*c2)->GetId(), false);
	ConnectControlSignals(widget1);
	ConnectControlSignals(widget2);
	ui->macroEditConditionLayout->insertWidget(pos1, widget1);
	ui->macroEditConditionLayout->insertWidget(pos2, widget2);
}

void AdvSceneSwitcher::MoveMacroConditionUp(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= (int)macro->Conditions().size()) {
		return;
	}

	SwapConditions(macro, idx, idx - 1);
	HighlightCondition(idx - 1);
}

void AdvSceneSwitcher::MoveMacroConditionDown(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Conditions().size() - 1) {
		return;
	}

	SwapConditions(macro, idx, idx + 1);
	HighlightCondition(idx + 1);
}
