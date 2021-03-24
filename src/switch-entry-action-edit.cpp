#include "headers/switch-entry-action-edit.hpp"

std::vector<std::string> actionTypes{"AdvSceneSwitcher.logic.and",
				     "AdvSceneSwitcher.logic.or",
				     "AdvSceneSwitcher.logic.andNot",
				     "AdvSceneSwitcher.logic.orNot"};

static inline void populateActionSelection(QComboBox *list, bool root = false)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.c_str()));
	}
}

SwitchEntryActionEdit::SwitchEntryActionEdit(QWidget *parent,
					     SceneSequenceSwitch *entryData)
{
	this->setParent(parent);

	_logicSelection = new QComboBox();
	_conditionSelection = new QComboBox();
	_conditionLayout = new QVBoxLayout();
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

	populateActionSelection(_logicSelection, !parent);

	QHBoxLayout *logicLayout = new QHBoxLayout;
	logicLayout->addWidget(_logicSelection);
	logicLayout->addStretch();

	QHBoxLayout *conditionTypeSelectionLayout = new QHBoxLayout;
	conditionTypeSelectionLayout->addWidget(_conditionSelection);
	conditionTypeSelectionLayout->addStretch();

	_groupLayout->addLayout(logicLayout);
	_groupLayout->addLayout(conditionTypeSelectionLayout);
	_groupLayout->addLayout(_conditionLayout);

	QHBoxLayout *controlsLayout = new QHBoxLayout;
	controlsLayout->addWidget(_extend);
	controlsLayout->addWidget(_reduce);
	controlsLayout->addStretch();
	_groupLayout->addLayout(controlsLayout);

	_groupLayout->addLayout(_childLayout);
	_group->setLayout(_groupLayout);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->addWidget(_group);
	setLayout(mainLayout);

	//_entryData = entryData;
	UpdateEntryData();

	_loading = false;
}

void SwitchEntryActionEdit::ActionSelectionChanged(int idx) {}
void SwitchEntryActionEdit::UpdateEntryData() {}

void SwitchEntryActionEdit::ExtendClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	//std::lock_guard<std::mutex> lock(switcher->m);
	//auto es = switchData->extend();

	//SwitchEntryEdit *ew = new SwitchEntryEdit(this->parentWidget());
	//_childLayout->addWidget(ew);
}

void SwitchEntryActionEdit::ReduceClicked()
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
