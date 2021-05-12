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
				 int type, bool startCollapsed)
	: QWidget(parent)
{
	_actionSelection = new QComboBox();
	_section = new Section(300);

	QWidget::connect(_actionSelection, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(ActionSelectionChanged(int)));

	populateActionSelection(_actionSelection);

	_section->AddHeaderWidget(_actionSelection);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_section);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData(type);

	_loading = false;
	_section->Collapse(startCollapsed);
}

void MacroActionEdit::ActionSelectionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->reset();
	*_entryData = MacroActionFactory::Create(idx);
	auto widget =
		MacroActionFactory::CreateWidget(idx, window(), *_entryData);
	_section->SetContent(widget);
	_section->Collapse(false);
}

void MacroActionEdit::UpdateEntryData(int type)
{
	_actionSelection->setCurrentIndex(type);
	auto widget =
		MacroActionFactory::CreateWidget(type, window(), *_entryData);
	_section->SetContent(widget);
}

void MacroActionEdit::Collapse(bool collapsed)
{
	_section->Collapse(collapsed);
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
	newEntry->Collapse(false);
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
