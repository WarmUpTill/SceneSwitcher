#include "advanced-scene-switcher.hpp"
#include "switcher-data.hpp"
#include "macro-condition-edit.hpp"
#include "macro-condition-scene.hpp"
#include "section.hpp"
#include "utility.hpp"

namespace advss {

std::map<std::string, MacroConditionInfo> &MacroConditionFactory::GetMap()
{
	static std::map<std::string, MacroConditionInfo> _methods;
	return _methods;
}

bool MacroConditionFactory::Register(const std::string &id,
				     MacroConditionInfo info)
{
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroCondition>
MacroConditionFactory::Create(const std::string &id, Macro *m)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createFunc(m);
	}
	return nullptr;
}

QWidget *
MacroConditionFactory::CreateWidget(const std::string &id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createWidgetFunc(parent, cond);
	}
	return nullptr;
}

std::string MacroConditionFactory::GetConditionName(const std::string &id)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return "unknown condition";
}

std::string MacroConditionFactory::GetIdByName(const QString &name)
{
	for (auto it : GetMap()) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool MacroConditionFactory::UsesDurationModifier(const std::string &id)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._useDurationModifier;
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
	for (auto &[_, condition] :
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
	const std::string &id, bool root)
	: MacroSegmentEdit(switcher->macroProperties._highlightConditions,
			   parent),
	  _logicSelection(new QComboBox()),
	  _conditionSelection(new FilterComboBox()),
	  _dur(new DurationModifierEdit()),
	  _entryData(entryData),
	  _isRoot(root)
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
	QWidget::connect(window(), SIGNAL(HighlightConditionsChanged(bool)),
			 this, SLOT(EnableHighlight(bool)));

	populateLogicSelection(_logicSelection, root);
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

	auto lock = LockContext();
	(*_entryData)->SetLogicType(type);
}

bool MacroConditionEdit::IsRootNode()
{
	return _isRoot;
}

void MacroConditionEdit::SetLogicSelection()
{
	auto logic = (*_entryData)->GetLogicType();
	if (IsRootNode()) {
		_logicSelection->setCurrentIndex(static_cast<int>(logic));
	} else {
		_logicSelection->setCurrentIndex(static_cast<int>(logic) -
						 logic_root_offset);
	}
}

void MacroConditionEdit::SetRootNode(bool root)
{
	_isRoot = root;
	const QSignalBlocker blocker(_logicSelection);
	_logicSelection->clear();
	populateLogicSelection(_logicSelection, root);
	SetLogicSelection();
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
	SetLogicSelection();
	_section->SetContent(widget, (*_entryData)->GetCollapsed());

	_dur->setVisible(MacroConditionFactory::UsesDurationModifier(id));
	auto modifier = (*_entryData)->GetDurationModifier();
	_dur->SetValue(modifier);
	SetFocusPolicyOfWidgets();
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

std::shared_ptr<MacroSegment> MacroConditionEdit::Data()
{
	return *_entryData;
}

void AdvSceneSwitcher::AddMacroCondition(int idx)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > (int)macro->Conditions().size()) {
		return;
	}

	std::string id;
	LogicType logic;
	if (idx >= 1) {
		id = macro->Conditions().at(idx - 1)->GetId();
		if (idx == 1) {
			logic = LogicType::OR;
		} else {
			logic = macro->Conditions().at(idx - 1)->GetLogicType();
		}
	} else {
		MacroConditionScene temp(macro.get());
		id = temp.GetId();
		logic = LogicType::ROOT_NONE;
	}
	{
		auto lock = LockContext();
		auto cond = macro->Conditions().emplace(
			macro->Conditions().begin() + idx,
			MacroConditionFactory::Create(id, macro.get()));
		if (idx - 1 >= 0) {
			auto data = obs_data_create();
			macro->Conditions().at(idx - 1)->Save(data);
			macro->Conditions().at(idx)->Load(data);
			obs_data_release(data);
		}
		(*cond)->SetLogicType(logic);
		macro->UpdateConditionIndices();
		ui->conditionsList->Insert(
			idx,
			new MacroConditionEdit(this, &macro->Conditions()[idx],
					       id, idx == 0));
		SetConditionData(*macro);
	}
	HighlightCondition(idx);
	emit(MacroSegmentOrderChanged());
}

void AdvSceneSwitcher::on_conditionAdd_clicked()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (currentConditionIdx == -1) {
		AddMacroCondition((int)macro->Conditions().size());
	} else {
		AddMacroCondition(currentConditionIdx + 1);
	}
	if (currentConditionIdx != -1) {
		MacroConditionSelectionChanged(currentConditionIdx + 1);
	}
	ui->conditionsList->SetHelpMsgVisible(false);
}

void AdvSceneSwitcher::RemoveMacroCondition(int idx)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Conditions().size()) {
		return;
	}

	{
		auto lock = LockContext();
		ui->conditionsList->Remove(idx);
		macro->Conditions().erase(macro->Conditions().begin() + idx);
		macro->UpdateConditionIndices();
		if (idx == 0 && macro->Conditions().size() > 0) {
			auto newRoot = macro->Conditions().at(0);
			newRoot->SetLogicType(LogicType::ROOT_NONE);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(0))
				->SetRootNode(true);
		}
		SetConditionData(*macro);
	}
	MacroConditionSelectionChanged(-1);
	lastInteracted = MacroSection::CONDITIONS;
	emit(MacroSegmentOrderChanged());
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	if (currentConditionIdx == -1) {
		auto macro = GetSelectedMacro();
		if (!macro) {
			return;
		}
		RemoveMacroCondition((int)macro->Conditions().size() - 1);
	} else {
		RemoveMacroCondition(currentConditionIdx);
	}
	MacroConditionSelectionChanged(-1);
}

void AdvSceneSwitcher::on_conditionTop_clicked()
{
	if (currentConditionIdx == -1) {
		return;
	}
	MacroConditionReorder(0, currentConditionIdx);
	MacroConditionSelectionChanged(0);
}

void AdvSceneSwitcher::on_conditionUp_clicked()
{
	if (currentConditionIdx == -1 || currentConditionIdx == 0) {
		return;
	}
	MoveMacroConditionUp(currentConditionIdx);
	MacroConditionSelectionChanged(currentConditionIdx - 1);
}

void AdvSceneSwitcher::on_conditionDown_clicked()
{
	if (currentConditionIdx == -1 ||
	    currentConditionIdx ==
		    ui->conditionsList->ContentLayout()->count() - 1) {
		return;
	}
	MoveMacroConditionDown(currentConditionIdx);
	MacroConditionSelectionChanged(currentConditionIdx + 1);
}

void AdvSceneSwitcher::on_conditionBottom_clicked()
{
	if (currentConditionIdx == -1) {
		return;
	}
	const int newIdx = ui->conditionsList->ContentLayout()->count() - 1;
	MacroConditionReorder(newIdx, currentConditionIdx);
	MacroConditionSelectionChanged(newIdx);
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
	auto lock = LockContext();
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

	auto widget1 = static_cast<MacroConditionEdit *>(
		ui->conditionsList->ContentLayout()->takeAt(pos1)->widget());
	auto widget2 = static_cast<MacroConditionEdit *>(
		ui->conditionsList->ContentLayout()->takeAt(pos2 - 1)->widget());
	ui->conditionsList->Insert(pos1, widget2);
	ui->conditionsList->Insert(pos2, widget1);
	SetConditionData(*m);
	widget2->SetRootNode(root);
	widget1->SetRootNode(false);
	emit(MacroSegmentOrderChanged());
}

void AdvSceneSwitcher::MoveMacroConditionUp(int idx)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= (int)macro->Conditions().size()) {
		return;
	}

	SwapConditions(macro.get(), idx, idx - 1);
	HighlightCondition(idx - 1);
}

void AdvSceneSwitcher::MoveMacroConditionDown(int idx)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Conditions().size() - 1) {
		return;
	}

	SwapConditions(macro.get(), idx, idx + 1);
	HighlightCondition(idx + 1);
}

void AdvSceneSwitcher::MacroConditionSelectionChanged(int idx)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	ui->conditionsList->SetSelection(idx);
	ui->actionsList->SetSelection(-1);

	if (idx < 0 || (unsigned)idx >= macro->Conditions().size()) {
		currentConditionIdx = -1;
	} else {
		currentConditionIdx = idx;
		lastInteracted = MacroSection::CONDITIONS;
	}
	currentActionIdx = -1;
	HighlightControls();
}

void AdvSceneSwitcher::MacroConditionReorder(int to, int from)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (to == from || from < 0 || from > (int)macro->Conditions().size() ||
	    to < 0 || to > (int)macro->Conditions().size()) {
		return;
	}
	{
		auto lock = LockContext();
		auto condition = macro->Conditions().at(from);
		if (to == 0) {
			condition->SetLogicType(LogicType::ROOT_NONE);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(from))
				->SetRootNode(true);
			macro->Conditions().at(0)->SetLogicType(LogicType::AND);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(0))
				->SetRootNode(false);
		}
		if (from == 0) {
			condition->SetLogicType(LogicType::AND);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(from))
				->SetRootNode(false);
			macro->Conditions().at(1)->SetLogicType(
				LogicType::ROOT_NONE);
			static_cast<MacroConditionEdit *>(
				ui->conditionsList->WidgetAt(1))
				->SetRootNode(true);
		}
		macro->Conditions().erase(macro->Conditions().begin() + from);
		macro->Conditions().insert(macro->Conditions().begin() + to,
					   condition);
		macro->UpdateConditionIndices();
		ui->conditionsList->ContentLayout()->insertItem(
			to, ui->conditionsList->ContentLayout()->takeAt(from));
		SetConditionData(*macro);
	}
	HighlightCondition(to);
	emit(MacroSegmentOrderChanged());
}

} // namespace advss
