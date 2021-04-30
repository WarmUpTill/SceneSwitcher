#include "headers/macro-action-edit.hpp"
#include "headers/macro-action-switch-scene.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

std::map<int, MacroActionInfo> MacroActionFactory::_methods;

bool MacroActionFactory::Register(int id, MacroActionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const int id)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createFunc();

	return nullptr;
}

QWidget *MacroActionFactory::CreateWidget(const int id, QWidget *parent,
					  std::shared_ptr<MacroAction> action)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createWidgetFunc(parent, action);

	return nullptr;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : MacroActionFactory::GetActionTypes()) {
		list->addItem(obs_module_text(entry.second._name.c_str()));
	}
}

MacroActionEdit::MacroActionEdit(QWidget *parent,
				 std::shared_ptr<MacroAction> *entryData,
				 int type)
	: QWidget(parent)
{
	_actionSelection = new QComboBox();
	_actionWidgetLayout = new QVBoxLayout();
	_group = new QGroupBox();
	_groupLayout = new QVBoxLayout;

	QWidget::connect(_actionSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ActionSelectionChanged(int)));

	populateActionSelection(_actionSelection);

	QHBoxLayout *actionLayout = new QHBoxLayout;
	actionLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.macroTab.edit.action")));
	actionLayout->addWidget(_actionSelection);
	actionLayout->addStretch();

	_groupLayout->addLayout(actionLayout);
	_groupLayout->addLayout(_actionWidgetLayout);
	_group->setLayout(_groupLayout);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->addWidget(_group);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData(type);

	_loading = false;
}

void MacroActionEdit::ActionSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->reset();
	*_entryData = MacroActionFactory::Create(idx);
	clearLayout(_actionWidgetLayout);
	auto widget = MacroActionFactory::CreateWidget(idx, this, *_entryData);
	_actionWidgetLayout->addWidget(widget);
}

void MacroActionEdit::UpdateEntryData(int type)
{
	_actionSelection->setCurrentIndex(type);
	clearLayout(_actionWidgetLayout);
	auto widget = MacroActionFactory::CreateWidget(type, this, *_entryData);
	_actionWidgetLayout->addWidget(widget);
}

void AdvSceneSwitcher::on_actionAdd_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	macro->Actions().emplace_back(MacroActionFactory::Create(0));
	auto newEntry = new MacroActionEdit(this, &macro->Actions().back(), 0);
	ui->macroEditActionLayout->addWidget(newEntry);
	ui->macroEditActionHelp->setVisible(false);
}

void AdvSceneSwitcher::on_actionRemove_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	if (macro->Actions().empty()) {
		return;
	}
	macro->Actions().pop_back();

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
