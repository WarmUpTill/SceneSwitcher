#include "headers/macro-condition-edit.hpp"

bool MacroConditionEdit::enableAdvancedLogic = false;

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
			if (static_cast<int>(entry.first) >= 100) {
				list->addItem(obs_module_text(
					entry.second._name.c_str()));
			}
		}
	} else {
		for (auto entry : MacroCondition::logicTypes) {
			if (static_cast<int>(entry.first) < 100) {
				list->addItem(obs_module_text(
					entry.second._name.c_str()));
			}
		}
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.c_str()));
	}
}

MacroConditionEdit::MacroConditionEdit(QWidget *parent,
				       SceneSequenceSwitch *entryData)
{
	this->setParent(parent);

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

	populateLogicSelection(_logicSelection, !parent);
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

	//_entryData = entryData;
	UpdateEntryData();

	_isRoot = parent == nullptr;
	_loading = false;
}

void MacroConditionEdit::LogicSelectionChanged(int idx)
{
	if (IsRootNode()) {
		LogicType type = static_cast<LogicType>(idx + 100);
		_group->setTitle(obs_module_text(
			MacroCondition::logicTypes[type]._name.c_str()));
	} else {
		LogicType type = static_cast<LogicType>(idx);
		_group->setTitle(obs_module_text(
			MacroCondition::logicTypes[type]._name.c_str()));
	}
}

bool MacroConditionEdit::IsRootNode()
{
	return _isRoot;
}

void MacroConditionEdit::UpdateEntryData() {}

void MacroConditionEdit::ConditionSelectionChanged(int idx)
{
	clearLayout(_conditionWidgetLayout);

	if (idx == 0) {
		auto widget = new MacroConditionSceneEdit();
		_conditionWidgetLayout->addWidget(widget);
	}
}

void MacroConditionEdit::ExtendClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	//std::lock_guard<std::mutex> lock(switcher->m);
	//auto es = switchData->extend();

	MacroConditionEdit *ew = new MacroConditionEdit(this->parentWidget());
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
	MacroConditionEdit *newEntry;

	int count = ui->macroEditConditionLayout->count();
	auto item = ui->macroEditConditionLayout->itemAt(count - 1);

	if (item) {
		auto widget = item->widget();
		newEntry = new MacroConditionEdit(widget);
	} else {
		newEntry = new MacroConditionEdit();
	}

	ui->macroEditConditionLayout->addWidget(newEntry);
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	int count = ui->macroEditConditionLayout->count();
	auto item = ui->macroEditConditionLayout->takeAt(count - 1);

	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}
}
