#include "headers/switch-entry-condition-edit.hpp"

bool SwitchEntryConditionEdit::enableAdvancedLogic = false;

std::vector<std::string> logicTypesRoot{"AdvSceneSwitcher.logic.none",
					"AdvSceneSwitcher.logic.not"};

std::vector<std::string> logicTypes{"AdvSceneSwitcher.logic.and",
				    "AdvSceneSwitcher.logic.or",
				    "AdvSceneSwitcher.logic.andNot",
				    "AdvSceneSwitcher.logic.orNot"};

std::vector<std::string> conditionTypes{
	"AdvSceneSwitcher.condition.scene", "AdvSceneSwitcher.logic.media",
	"AdvSceneSwitcher.logic.audio",     "AdvSceneSwitcher.logic.video",
	"AdvSceneSwitcher.logic.time",      "AdvSceneSwitcher.logic.region",
	"AdvSceneSwitcher.logic.window",    "AdvSceneSwitcher.logic.process",
	"AdvSceneSwitcher.logic.file",      "AdvSceneSwitcher.logic.idle"};

static inline void populateLogicSelection(QComboBox *list, bool root = false)
{
	if (root) {
		for (auto entry : logicTypesRoot) {
			list->addItem(obs_module_text(entry.c_str()));
		}
	} else {
		for (auto entry : logicTypes) {
			list->addItem(obs_module_text(entry.c_str()));
		}
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.c_str()));
	}
}

SwitchEntryConditionEdit::SwitchEntryConditionEdit(
	QWidget *parent, SceneSequenceSwitch *entryData)
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

void SwitchEntryConditionEdit::LogicSelectionChanged(int idx)
{
	if (IsRootNode()) {
		_group->setTitle(obs_module_text(logicTypesRoot[idx].c_str()));
	} else {
		_group->setTitle(obs_module_text(logicTypes[idx].c_str()));
	}
}

bool SwitchEntryConditionEdit::IsRootNode()
{
	return _isRoot;
}

void SwitchEntryConditionEdit::UpdateEntryData() {}

void SwitchEntryConditionEdit::ConditionSelectionChanged(int idx)
{
	clearLayout(_conditionWidgetLayout);

	if (idx == 3) {
		auto test = new SceneSequenceSwitch;
		auto widget = new SequenceWidget(this, test);
		_conditionWidgetLayout->addWidget(widget);
	}
}

void SwitchEntryConditionEdit::ExtendClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	//std::lock_guard<std::mutex> lock(switcher->m);
	//auto es = switchData->extend();

	SwitchEntryConditionEdit *ew =
		new SwitchEntryConditionEdit(this->parentWidget());
	_childLayout->addWidget(ew);
}

void SwitchEntryConditionEdit::ReduceClicked()
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

#include "headers/switch-entry-action-edit.hpp"

void AdvSceneSwitcher::setupTestTab()
{
	SwitchEntryConditionEdit *test = new SwitchEntryConditionEdit();
	ui->reworkEditConditionLayout->addWidget(test);

	auto *test2 = new SwitchEntryActionEdit();
	ui->reworkEditActionLayout->addWidget(test2);

	ui->switchEntryEditConditionHelp->hide();
	ui->switchEntryEditActionHelp->hide();
}

void AdvSceneSwitcher::on_conditionAdd_clicked()
{
	SwitchEntryConditionEdit *newEntry;

	int count = ui->reworkEditConditionLayout->count();
	auto item = ui->reworkEditConditionLayout->itemAt(count - 1);

	if (item) {
		auto widget = item->widget();
		newEntry = new SwitchEntryConditionEdit(widget);
	} else {
		newEntry = new SwitchEntryConditionEdit();
	}

	ui->reworkEditConditionLayout->addWidget(newEntry);
}

void AdvSceneSwitcher::on_conditionRemove_clicked()
{
	int count = ui->reworkEditConditionLayout->count();
	auto item = ui->reworkEditConditionLayout->takeAt(count - 1);

	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}
}
