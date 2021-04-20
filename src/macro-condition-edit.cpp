#include "headers/macro-condition-edit.hpp"

bool MacroConditionEdit::enableAdvancedLogic = false;

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
MacroConditionFactory::CreateWidget(const int id,
				    std::shared_ptr<MacroCondition> cond)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createWidgetFunc(cond);

	return nullptr;
}

// TODO: DELETE ME :)
std::vector<std::string> conditionTypes{
	"AdvSceneSwitcher.condition.scene", "AdvSceneSwitcher.logic.media",
	"AdvSceneSwitcher.logic.audio",     "AdvSceneSwitcher.logic.video",
	"AdvSceneSwitcher.logic.time",      "AdvSceneSwitcher.logic.region",
	"AdvSceneSwitcher.logic.window",    "AdvSceneSwitcher.logic.process",
	"AdvSceneSwitcher.logic.file",      "AdvSceneSwitcher.logic.idle"};

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
	std::shared_ptr<MacroCondition> *entryData, int type, bool root)
{
	_logicSelection = new QComboBox();
	_conditionSelection = new QComboBox();
	_conditionWidgetLayout = new QVBoxLayout();
	_group = new QGroupBox();
	_groupLayout = new QVBoxLayout;
	_childLayout = new QVBoxLayout;
	_extend = new QPushButton();
	_reduce = new QPushButton();

	_extend->setProperty("themeID",
			     QVariant(QStringLiteral("addIconSmall")));
	_reduce->setProperty("themeID",
			     QVariant(QStringLiteral("removeIconSmall")));

	_extend->setMaximumSize(22, 22);
	_reduce->setMaximumSize(22, 22);

	QWidget::connect(_logicSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(LogicSelectionChanged(int)));
	QWidget::connect(_conditionSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ConditionSelectionChanged(int)));
	QWidget::connect(_extend, SIGNAL(clicked()), this,
			 SLOT(ExtendClicked()));
	QWidget::connect(_reduce, SIGNAL(clicked()), this,
			 SLOT(ReduceClicked()));

	populateLogicSelection(_logicSelection, root);
	populateConditionSelection(_conditionSelection);

	QHBoxLayout *logicLayout = new QHBoxLayout;
	logicLayout->addWidget(_logicSelection);
	logicLayout->addStretch();

	QHBoxLayout *conditionTypeSelectionLayout = new QHBoxLayout;
	conditionTypeSelectionLayout->addWidget(_conditionSelection);
	conditionTypeSelectionLayout->addStretch();

	_groupLayout->addLayout(logicLayout);
	_groupLayout->addLayout(conditionTypeSelectionLayout);
	_groupLayout->addLayout(_conditionWidgetLayout);

	QHBoxLayout *controlsLayout = new QHBoxLayout;
	controlsLayout->addWidget(_extend);
	controlsLayout->addWidget(_reduce);
	controlsLayout->addStretch();
	_groupLayout->addLayout(controlsLayout);

	if (!enableAdvancedLogic) {
		_extend->hide();
		_reduce->hide();
	}

	_groupLayout->addLayout(_childLayout);
	_group->setLayout(_groupLayout);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->addWidget(_group);
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
	_entryData->get()->SetLogicType(type);
	SetGroupTitle();
}

bool MacroConditionEdit::IsRootNode()
{
	return _isRoot;
}

void MacroConditionEdit::SetGroupTitle()
{
	_group->setTitle(obs_module_text(
		MacroCondition::logicTypes[_entryData->get()->GetLogicType()]
			._name.c_str()));
}

void MacroConditionEdit::UpdateEntryData(int type)
{
	clearLayout(_conditionWidgetLayout);
	auto widget = MacroConditionFactory::CreateWidget(type, *_entryData);
	_conditionWidgetLayout->addWidget(widget);

	auto logic = _entryData->get()->GetLogicType();
	if (IsRootNode()) {
		_logicSelection->setCurrentIndex(static_cast<int>(logic));
	} else {
		_logicSelection->setCurrentIndex(static_cast<int>(logic) -
						 logic_root_offset);
	}
	SetGroupTitle();
}

void MacroConditionEdit::ConditionSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->reset();
	*_entryData = MacroConditionFactory::Create(idx);
	clearLayout(_conditionWidgetLayout);
	auto widget = MacroConditionFactory::CreateWidget(idx, *_entryData);
	_conditionWidgetLayout->addWidget(widget);
}

void MacroConditionEdit::ExtendClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	//std::lock_guard<std::mutex> lock(switcher->m);
	//auto es = switchData->extend();

	MacroConditionEdit *ew = new MacroConditionEdit(nullptr, false);
	_childLayout->addWidget(ew);
}

void MacroConditionEdit::ReduceClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	//std::lock_guard<std::mutex> lock(switcher->m);
	//switchData->reduce();

	int count = _childLayout->count();
	auto item = _childLayout->takeAt(count - 1);
	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}
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
	auto newEntry =
		new MacroConditionEdit(&macro->Conditions().back(), 0, root);
	ui->macroEditConditionLayout->addWidget(newEntry);
	ui->macroEditConditionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
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
