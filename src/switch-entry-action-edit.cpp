#include "headers/switch-entry-action-edit.hpp"

std::vector<std::string> actionTypes{"AdvSceneSwitcher.action.switchScene",
				     "AdvSceneSwitcher.logic.muteSource",
				     "AdvSceneSwitcher.logic.unmuteSource",
				     "AdvSceneSwitcher.logic.blablabla"};

static inline void populateActionSelection(QComboBox *list, bool root = false)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.c_str()));
	}
}

SwitchEntryActionEdit::SwitchEntryActionEdit(
	QWidget *parent, std::deque<SceneSequenceSwitch> *entryData)
{
	this->setParent(parent);

	_actionSelection = new QComboBox();
	_actionWidgetLayout = new QVBoxLayout();
	_group = new QGroupBox();
	_groupLayout = new QVBoxLayout;

	QWidget::connect(_actionSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(populateActionSelection(int)));

	populateActionSelection(_actionSelection, !parent);

	QHBoxLayout *actionLayout = new QHBoxLayout;
	actionLayout->addWidget(_actionSelection);
	actionLayout->addStretch();

	_groupLayout->addLayout(actionLayout);
	_groupLayout->addLayout(_actionWidgetLayout);
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

void AdvSceneSwitcher::on_actionAdd_clicked()
{
	SwitchEntryActionEdit *newEntry;

	int count = ui->reworkEditActionLayout->count();
	auto item = ui->reworkEditActionLayout->itemAt(count - 1);

	if (item) {
		auto widget = item->widget();
		newEntry = new SwitchEntryActionEdit(widget);
	} else {
		newEntry = new SwitchEntryActionEdit();
	}

	ui->reworkEditActionLayout->addWidget(newEntry);
}

void AdvSceneSwitcher::on_actionRemove_clicked()
{
	int count = ui->reworkEditActionLayout->count();
	auto item = ui->reworkEditActionLayout->takeAt(count - 1);

	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}
}
