#include "headers/macro-action-edit.hpp"
#include "headers/macro-action-scene-switch.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

std::map<std::string, MacroActionInfo> MacroActionFactory::_methods;

bool MacroActionFactory::Register(const std::string &id, MacroActionInfo info)
{
	if (auto it = _methods.find(id); it == _methods.end()) {
		_methods[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const std::string &id)
{
	if (auto it = _methods.find(id); it != _methods.end())
		return it->second._createFunc();

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
}

MacroActionEdit::MacroActionEdit(QWidget *parent,
				 std::shared_ptr<MacroAction> *entryData,
				 const std::string &id)
	: MacroSegmentEdit(parent), _entryData(entryData)
{
	_actionSelection = new QComboBox();
	_section = new Section(300);
	_headerInfo = new QLabel();
	_controls = new MacroEntryControls();

	QWidget::connect(_actionSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ActionSelectionChanged(const QString &)));
	QWidget::connect(_section, &Section::Collapsed, this,
			 &MacroActionEdit::Collapsed);
	// Macro signals
	QWidget::connect(parent, SIGNAL(MacroAdded(const QString &)), this,
			 SIGNAL(MacroAdded(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SIGNAL(MacroRemoved(const QString &)));
	QWidget::connect(parent,
			 SIGNAL(MacroRenamed(const QString &, const QString)),
			 this,
			 SIGNAL(MacroRenamed(const QString &, const QString)));

	// Scene group signals
	QWidget::connect(parent, SIGNAL(SceneGroupAdded(const QString &)), this,
			 SIGNAL(SceneGroupAdded(const QString &)));
	QWidget::connect(parent, SIGNAL(SceneGroupRemoved(const QString &)),
			 this, SIGNAL(SceneGroupRemoved(const QString &)));
	QWidget::connect(
		parent,
		SIGNAL(SceneGroupRenamed(const QString &, const QString)), this,
		SIGNAL(SceneGroupRenamed(const QString &, const QString)));

	// Control signals
	QWidget::connect(_controls, SIGNAL(Add()), this, SLOT(Add()));
	QWidget::connect(_controls, SIGNAL(Remove()), this, SLOT(Remove()));
	QWidget::connect(_controls, SIGNAL(Up()), this, SLOT(Up()));
	QWidget::connect(_controls, SIGNAL(Down()), this, SLOT(Down()));

	populateActionSelection(_actionSelection);

	_section->AddHeaderWidget(_actionSelection);
	_section->AddHeaderWidget(_headerInfo);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_section);
	mainLayout->addWidget(_controls);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData(id);

	_loading = false;
}

void MacroActionEdit::enterEvent(QEvent *)
{
	_controls->Show(true);
}

void MacroActionEdit::leaveEvent(QEvent *)
{
	_controls->Show(false);
}

void MacroActionEdit::ActionSelectionChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::string id = MacroActionFactory::GetIdByName(text);
	HeaderInfoChanged("");

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->reset();
	*_entryData = MacroActionFactory::Create(id);
	auto widget = MacroActionFactory::CreateWidget(id, this, *_entryData);
	QWidget::connect(widget, SIGNAL(HeaderInfoChanged(const QString &)),
			 this, SLOT(HeaderInfoChanged(const QString &)));
	_section->SetContent(widget, false);
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

void MacroActionEdit::HeaderInfoChanged(const QString &text)
{
	_headerInfo->setVisible(!text.isEmpty());
	_headerInfo->setText(text);
}

void MacroActionEdit::Add()
{
	if (_entryData) {
		// Insert after current entry
		emit AddAt((*_entryData)->GetIndex() + 1);
	}
}

void MacroActionEdit::Remove()
{
	if (_entryData) {
		emit RemoveAt((*_entryData)->GetIndex());
	}
}

void MacroActionEdit::Up()
{
	if (_entryData) {
		emit UpAt((*_entryData)->GetIndex());
	}
}

void MacroActionEdit::Down()
{
	if (_entryData) {
		emit DownAt((*_entryData)->GetIndex());
	}
}

void MacroActionEdit::Collapsed(bool collapsed)
{
	if (_entryData) {
		(*_entryData)->SetCollapsed(collapsed);
	}
}

void AdvSceneSwitcher::AddMacroAction(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx > macro->Actions().size()) {
		return;
	}

	MacroActionSwitchScene temp;
	std::string id = temp.GetId();

	std::lock_guard<std::mutex> lock(switcher->m);
	auto action = macro->Actions().emplace(macro->Actions().begin() + idx,
					       MacroActionFactory::Create(id));
	macro->UpdateActionIndices();

	// All entry pointers in existing edit widgets after the new entry will
	// be invalidated by adding the new entry so we have to recreate them
	// and because I am lazy I am recreating every widget.
	//
	// If performance should become a concern this has to be revisited.
	SetEditMacro(*macro);
}

void AdvSceneSwitcher::on_actionAdd_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	AddMacroAction((int)macro->Actions().size());
	ui->macroEditActionHelp->setVisible(false);
}

void AdvSceneSwitcher::RemoveMacroAction(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= macro->Actions().size()) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	macro->Actions().erase(macro->Actions().begin() + idx);
	macro->UpdateActionIndices();

	// All entry pointers in existing edit widgets after the new entry will
	// be invalidated by adding the new entry so we have to recreate them
	// and because I am lazy I am recreating every widget.
	//
	// If performance should become a concern this has to be revisited.
	SetEditMacro(*macro);
}

void AdvSceneSwitcher::on_actionRemove_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	RemoveMacroAction((int)macro->Actions().size() - 1);
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

	auto item1 = ui->macroEditActionLayout->takeAt(pos1);
	auto item2 = ui->macroEditActionLayout->takeAt(pos2 - 1);
	deleteLayoutItem(item1);
	deleteLayoutItem(item2);
	auto widget1 = new MacroActionEdit(this, &(*a1), (*a1)->GetId());
	auto widget2 = new MacroActionEdit(this, &(*a2), (*a2)->GetId());
	ConnectControlSignals(widget1);
	ConnectControlSignals(widget2);
	ui->macroEditActionLayout->insertWidget(pos1, widget1);
	ui->macroEditActionLayout->insertWidget(pos2, widget2);
}

void AdvSceneSwitcher::MoveMacroActionUp(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 1 || idx >= macro->Actions().size()) {
		return;
	}

	SwapActions(macro, idx, idx - 1);
}

void AdvSceneSwitcher::MoveMacroActionDown(int idx)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (idx < 0 || idx >= macro->Actions().size() - 1) {
		return;
	}

	SwapActions(macro, idx, idx + 1);
}
