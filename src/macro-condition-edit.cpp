#include "headers/macro-condition-edit.hpp"

std::map<int, MacroConditionInfo> MacroConditionFactory::_methods;

bool MacroConditionFactory::Register(int id, MacroConditionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroCondition> MacroConditionFactory::Create(const int id)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createFunc();

	return nullptr;
}

QWidget *
MacroConditionFactory::CreateWidget(const int id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createWidgetFunc(parent, cond);

	return nullptr;
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
}

MacroConditionEdit::MacroConditionEdit(
	QWidget *parent, std::shared_ptr<MacroCondition> *entryData, int type,
	bool root)
	: QWidget(parent)
{
	_logicSelection = new QComboBox();
	_conditionSelection = new QComboBox();
	_section = new Section(300);

	QWidget::connect(_logicSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(LogicSelectionChanged(int)));
	QWidget::connect(_conditionSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ConditionSelectionChanged(int)));

	populateLogicSelection(_logicSelection, root);
	populateConditionSelection(_conditionSelection);

	_section->AddHeaderWidget(_logicSelection);
	_section->AddHeaderWidget(_conditionSelection);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_section);
	setLayout(mainLayout);

	_entryData = entryData;
	_isRoot = root;
	UpdateEntryData(type);
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

void MacroConditionEdit::UpdateEntryData(int type)
{
	_conditionSelection->setCurrentIndex(type);
	auto widget = MacroConditionFactory::CreateWidget(type, window(),
							  *_entryData);
	auto logic = (*_entryData)->GetLogicType();
	if (IsRootNode()) {
		_logicSelection->setCurrentIndex(static_cast<int>(logic));
	} else {
		_logicSelection->setCurrentIndex(static_cast<int>(logic) -
						 logic_root_offset);
	}
	_section->SetContent(widget);
}

void MacroConditionEdit::Collapse(bool collapsed)
{
	_section->Collapse(collapsed);
}

void MacroConditionEdit::ConditionSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	auto logic = (*_entryData)->GetLogicType();
	_entryData->reset();
	*_entryData = MacroConditionFactory::Create(idx);
	(*_entryData)->SetLogicType(logic);
	auto widget =
		MacroConditionFactory::CreateWidget(idx, window(), *_entryData);
	_section->SetContent(widget);
	_section->Collapse(false);
}

void AdvSceneSwitcher::on_conditionAdd_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	bool root = macro->Conditions().size() == 0;
	macro->Conditions().emplace_back(MacroConditionFactory::Create(0));
	auto logic = root ? LogicType::ROOT_NONE : LogicType::NONE;
	macro->Conditions().back()->SetLogicType(logic);
	auto newEntry = new MacroConditionEdit(
		this, &macro->Conditions().back(), 0, root);
	ui->macroEditConditionLayout->addWidget(newEntry);
	ui->macroEditConditionHelp->setVisible(false);
	newEntry->Collapse(false);
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	if (macro->Conditions().empty()) {
		return;
	}
	macro->Conditions().pop_back();

	int count = ui->macroEditConditionLayout->count();
	auto item = ui->macroEditConditionLayout->takeAt(count - 1);

	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}

	if (count == 1) {
		ui->macroEditConditionHelp->setVisible(true);
	}
}
