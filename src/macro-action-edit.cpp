#include "headers/macro-action-edit.hpp"
#include "headers/macro-action-scene-switch.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

std::map<std::string, MacroActionInfo> MacroActionFactory::_methods;

bool MacroActionFactory::Register(std::string id, MacroActionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const std::string id)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createFunc();

	return nullptr;
}

QWidget *MacroActionFactory::CreateWidget(const std::string id, QWidget *parent,
					  std::shared_ptr<MacroAction> action)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createWidgetFunc(parent, action);

	return nullptr;
}

std::string MacroActionFactory::GetActionName(const std::string id)
{
	if (auto it = _methods.find(id); it != _methods.end()) {
		return it->second._name;
	}
	return "unknown action";
}

std::string MacroActionFactory::GetIdByName(const QString &name)
{
	for (auto it : _methods) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : MacroActionFactory::GetActionTypes()) {
		list->addItem(obs_module_text(entry.second._name.c_str()));
	}
}

MacroActionEdit::MacroActionEdit(QWidget *parent,
				 std::shared_ptr<MacroAction> *entryData,
				 std::string id, bool startCollapsed)
	: QWidget(parent)
{
	_actionSelection = new QComboBox();
	_section = new Section(300);

	QWidget::connect(_actionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ActionSelectionChanged(const QString &)));

	populateActionSelection(_actionSelection);

	_section->AddHeaderWidget(_actionSelection);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_section);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData(id);

	_loading = false;
	_section->Collapse(startCollapsed);
}

void MacroActionEdit::ActionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::string id = MacroActionFactory::GetIdByName(text);

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->reset();
	*_entryData = MacroActionFactory::Create(id);
	auto widget =
		MacroActionFactory::CreateWidget(id, window(), *_entryData);
	_section->SetContent(widget);
	_section->Collapse(false);
}

void MacroActionEdit::UpdateEntryData(std::string id)
{
	_actionSelection->setCurrentText(
		obs_module_text(MacroActionFactory::GetActionName(id).c_str()));
	auto widget =
		MacroActionFactory::CreateWidget(id, window(), *_entryData);
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

	MacroActionSwitchScene temp;
	std::string id = temp.GetId();

	std::lock_guard<std::mutex> lock(switcher->m);
	macro->Actions().emplace_back(MacroActionFactory::Create(id));
	auto newEntry = new MacroActionEdit(this, &macro->Actions().back(), id);
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
