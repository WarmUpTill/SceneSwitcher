#include "headers/macro-action-edit.hpp"
#include "headers/macro-action-switch-scene.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

std::vector<std::string> actionTypes{"AdvSceneSwitcher.action.switchScene",
				     "AdvSceneSwitcher.action.wait",
				     "AdvSceneSwitcher.action.muteSource",
				     "AdvSceneSwitcher.action.unmuteSource",
				     "AdvSceneSwitcher.action.blablabla"};

static inline void populateActionSelection(QComboBox *list, bool root = false)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.c_str()));
	}
}

MacroActionEdit::MacroActionEdit(QWidget *parent,
				 std::deque<MacroAction> *entryData)
{
	this->setParent(parent);

	_actionSelection = new QComboBox();
	_actionWidgetLayout = new QVBoxLayout();
	_group = new QGroupBox();
	_groupLayout = new QVBoxLayout;

	QWidget::connect(_actionSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ActionSelectionChanged(int)));

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

void MacroActionEdit::ActionSelectionChanged(int idx)
{
	clearLayout(_actionWidgetLayout);

	if (idx == 0) {
		auto widget = new MacroActionSwitchSceneEdit();
		_actionWidgetLayout->addWidget(widget);
	}
}

void MacroActionEdit::UpdateEntryData() {}

void AdvSceneSwitcher::on_actionAdd_clicked()
{
	MacroActionEdit *newEntry;

	int count = ui->macroEditActionLayout->count();
	auto item = ui->macroEditActionLayout->itemAt(count - 1);

	if (item) {
		auto widget = item->widget();
		newEntry = new MacroActionEdit(widget);
	} else {
		newEntry = new MacroActionEdit();
	}

	ui->macroEditActionLayout->addWidget(newEntry);
	ui->macroEditActionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_actionRemove_clicked()
{
	int count = ui->macroEditActionLayout->count();
	auto item = ui->macroEditActionLayout->takeAt(count - 1);

	if (item) {
		auto widget = item->widget();
		if (widget) {
			widget->setVisible(false);
		}
		delete item;
	}

	if (count == 1) {
		ui->macroEditActionHelp->setVisible(true);
	}
}
