#include "headers/advanced-scene-switcher.hpp"
#include "headers/macro-action-edit.hpp"
#include "headers/macro-action-scene-switch.hpp"
#include "headers/section.hpp"
#include "headers/utility.hpp"

std::map<std::string, MacroActionInfo> MacroActionFactory::_methods;

bool MacroActionFactory::Register(const std::string &id, MacroActionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const std::string &id,
							Macro *m)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createFunc(m);

	return nullptr;
}

QWidget *MacroActionFactory::CreateWidget(const std::string &id,
					  QWidget *parent,
					  std::shared_ptr<MacroAction> action)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createWidgetFunc(parent, action);

	return nullptr;
}

std::string MacroActionFactory::GetActionName(const std::string &id)
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
	list->model()->sort(0);
}

MacroActionEdit::MacroActionEdit(QWidget *parent,
				 std::shared_ptr<MacroAction> *entryData,
				 const std::string &id)
	: MacroSegmentEdit(switcher->macroProperties._highlightActions, parent),
	  _entryData(entryData)
{
	_actionSelection = new QComboBox();

	QWidget::connect(_actionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ActionSelectionChanged(const QString &)));
	QWidget::connect(window(), SIGNAL(HighlightActionsChanged(bool)), this,
			 SLOT(EnableHighlight(bool)));

	populateActionSelection(_actionSelection);

	_section->AddHeaderWidget(_actionSelection);
	_section->AddHeaderWidget(_headerInfo);

	QVBoxLayout *actionLayout = new QVBoxLayout;
	actionLayout->setContentsMargins({7, 7, 7, 7});
	actionLayout->addWidget(_section);
	_contentLayout->addLayout(actionLayout);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	mainLayout->addWidget(_frame);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData(id);

	_loading = false;
}

void MacroActionEdit::ActionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto idx = _entryData->get()->GetIndex();
	auto macro = _entryData->get()->GetMacro();
	std::string id = MacroActionFactory::GetIdByName(text);
	HeaderInfoChanged("");

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->reset();
	*_entryData = MacroActionFactory::Create(id, macro);
	(*_entryData)->SetIndex(idx);
	auto widget = MacroActionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget);
	SetFocusPolicyOfWidgets();
}

void MacroActionEdit::UpdateEntryData(const std::string &id)
{
	_actionSelection->setCurrentText(
		obs_module_text(MacroActionFactory::GetActionName(id).c_str()));
	auto widget = MacroActionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	HeaderInfoChanged(
		QString::fromStdString((*_entryData)->GetShortDesc()));
	_section->SetContent(widget, (*_entryData)->GetCollapsed());
	SetFocusPolicyOfWidgets();
}

void MacroActionEdit::SetEntryData(std::shared_ptr<MacroAction> *data)
{
	_entryData = data;
}

MacroSegment *MacroActionEdit::Data()
{
	return _entryData->get();
}

void AdvSceneSwitcher::AddMacroAction(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > (int)macro->Actions().size()) {
		return;
	}

	std::string id;
	if (idx - 1 >= 0) {
		id = macro->Actions().at(idx - 1)->GetId();
	} else {
		MacroActionSwitchScene temp(nullptr);
		id = temp.GetId();
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		macro->Actions().emplace(macro->Actions().begin() + idx,
					 MacroActionFactory::Create(id, macro));
		if (idx - 1 >= 0) {
			auto data = obs_data_create();
			macro->Actions().at(idx - 1)->Save(data);
			macro->Actions().at(idx)->Load(data);
			obs_data_release(data);
		}
		macro->UpdateActionIndices();
		actionsList->Insert(
			idx,
			new MacroActionEdit(this, &macro->Actions()[idx], id));
		SetActionData(*macro);
	}
	HighlightAction(idx);
}

void AdvSceneSwitcher::on_actionAdd_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (currentActionIdx == -1) {
		auto macro = getSelectedMacro();
		if (!macro) {
			return;
		}
		AddMacroAction((int)macro->Actions().size());
	} else {
		AddMacroAction(currentActionIdx + 1);
	}
	if (currentActionIdx != -1) {
		MacroActionSelectionChanged(currentActionIdx + 1);
	}
	actionsList->SetHelpMsgVisible(false);
}

void AdvSceneSwitcher::RemoveMacroAction(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Actions().size()) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		macro->Actions().erase(macro->Actions().begin() + idx);
		switcher->abortMacroWait = true;
		switcher->macroWaitCv.notify_all();
		macro->UpdateActionIndices();
		actionsList->Remove(idx);
		SetActionData(*macro);
	}
	MacroActionSelectionChanged(-1);
}

void AdvSceneSwitcher::on_actionRemove_clicked()
{
	if (currentActionIdx == -1) {
		auto macro = getSelectedMacro();
		if (!macro) {
			return;
		}
		RemoveMacroAction((int)macro->Actions().size() - 1);
	} else {
		RemoveMacroAction(currentActionIdx);
	}
	MacroActionSelectionChanged(-1);
}

void AdvSceneSwitcher::on_actionUp_clicked()
{
	if (currentActionIdx == -1) {
		return;
	}
	MoveMacroActionUp(currentActionIdx);
	MacroActionSelectionChanged(currentActionIdx - 1);
}
void AdvSceneSwitcher::on_actionDown_clicked()
{
	if (currentActionIdx == -1) {
		return;
	}
	MoveMacroActionDown(currentActionIdx);
	MacroActionSelectionChanged(currentActionIdx + 1);
}

void AdvSceneSwitcher::SwapActions(Macro *m, int pos1, int pos2)
{
	if (pos1 == pos2) {
		return;
	}
	if (pos1 > pos2) {
		std::swap(pos1, pos2);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	iter_swap(m->Actions().begin() + pos1, m->Actions().begin() + pos2);
	m->UpdateActionIndices();

	auto a1 = m->Actions().begin() + pos1;
	auto a2 = m->Actions().begin() + pos2;

	auto widget1 = static_cast<MacroActionEdit *>(
		actionsList->ContentLayout()->takeAt(pos1)->widget());
	auto widget2 = static_cast<MacroActionEdit *>(
		actionsList->ContentLayout()->takeAt(pos2 - 1)->widget());
	actionsList->Insert(pos1, widget2);
	actionsList->Insert(pos2, widget1);
	SetActionData(*m);
}

void AdvSceneSwitcher::MoveMacroActionUp(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= (int)macro->Actions().size()) {
		return;
	}

	SwapActions(macro, idx, idx - 1);
	HighlightAction(idx - 1);
}

void AdvSceneSwitcher::MoveMacroActionDown(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= (int)macro->Actions().size() - 1) {
		return;
	}

	SwapActions(macro, idx, idx + 1);
	HighlightAction(idx + 1);
}

void AdvSceneSwitcher::MacroActionSelectionChanged(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	actionsList->SetSelection(idx);
	conditionsList->SetSelection(-1);

	if (idx < 0 || (unsigned)idx >= macro->Actions().size()) {
		currentActionIdx = -1;
	} else {
		currentActionIdx = idx;
	}
	currentConditionIdx = -1;
	HighlightControls();
}

void AdvSceneSwitcher::MacroActionReorder(int to, int from)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (from < 0 || from > (int)macro->Actions().size() || to < 0 ||
	    to > (int)macro->Actions().size()) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto action = macro->Actions().at(from);
		macro->Actions().erase(macro->Actions().begin() + from);
		macro->Actions().insert(macro->Actions().begin() + to, action);
		macro->UpdateActionIndices();
		SetActionData(*macro);
	}
	HighlightAction(to);
}
