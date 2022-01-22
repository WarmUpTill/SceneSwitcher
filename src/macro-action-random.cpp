#include "headers/macro-action-random.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <cstdlib>

const std::string MacroActionRandom::id = "random";

bool MacroActionRandom::_registered = MacroActionFactory::Register(
	MacroActionRandom::id,
	{MacroActionRandom::Create, MacroActionRandomEdit::Create,
	 "AdvSceneSwitcher.action.random"});

std::vector<MacroRef> getNextMacro(std::vector<MacroRef> &macros,
				   MacroRef &lastRandomMacro)
{
	std::vector<MacroRef> res;
	if (macros.size() == 1) {
		if (macros[0]->Paused()) {
			return res;
		} else {
			return macros;
		}
	}

	for (auto &m : macros) {
		if (m.get() && !m->Paused() &&
		    !(lastRandomMacro.get() == m.get())) {
			res.push_back(m);
		}
	}
	return res;
}

bool MacroActionRandom::PerformAction()
{
	if (_macros.size() == 0) {
		return true;
	}

	auto macros = getNextMacro(_macros, lastRandomMacro);
	if (macros.size() == 0) {
		return true;
	}
	if (macros.size() == 1) {
		lastRandomMacro = macros[0];
		return macros[0]->PerformAction();
	}

	size_t idx = std::rand() % (macros.size());
	lastRandomMacro = macros[idx];
	return macros[idx]->PerformAction();
}

void MacroActionRandom::LogAction()
{
	vblog(LOG_INFO, "running random macro");
}

bool MacroActionRandom::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_array_t *args = obs_data_array_create();
	for (auto &m : _macros) {
		obs_data_t *array_obj = obs_data_create();
		m.Save(array_obj);
		obs_data_array_push_back(args, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "macros", args);
	obs_data_array_release(args);
	return true;
}

bool MacroActionRandom::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	obs_data_array_t *args = obs_data_get_array(obj, "macros");
	size_t count = obs_data_array_count(args);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(args, i);
		MacroRef ref;
		ref.Load(array_obj);
		_macros.push_back(ref);
		obs_data_release(array_obj);
	}
	obs_data_array_release(args);
	return true;
}

MacroActionRandomEdit::MacroActionRandomEdit(
	QWidget *parent, std::shared_ptr<MacroActionRandom> entryData)
	: QWidget(parent)
{
	_macroList = new QListWidget();
	_macroList->setSortingEnabled(true);
	_add = new QPushButton();
	_add->setMaximumSize(QSize(22, 22));
	_add->setProperty("themeID",
			  QVariant(QString::fromUtf8("addIconSmall")));
	_add->setFlat(true);
	_remove = new QPushButton();
	_remove->setMaximumSize(QSize(22, 22));
	_remove->setProperty("themeID",
			     QVariant(QString::fromUtf8("removeIconSmall")));
	_remove->setFlat(true);

	QWidget::connect(_add, SIGNAL(clicked()), this, SLOT(AddMacro()));
	QWidget::connect(_remove, SIGNAL(clicked()), this, SLOT(RemoveMacro()));
	QWidget::connect(_macroList, SIGNAL(currentRowChanged(int)), this,
			 SLOT(MacroSelectionChanged(int)));
	QWidget::connect(window(),
			 SIGNAL(MacroRenamed(const QString &, const QString &)),
			 this,
			 SLOT(MacroRename(const QString &, const QString &)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.random.entry"),
		     entryLayout, widgetPlaceholders);

	auto *argButtonLayout = new QHBoxLayout;
	argButtonLayout->addWidget(_add);
	argButtonLayout->addWidget(_remove);
	argButtonLayout->addStretch();

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_macroList);
	mainLayout->addLayout(argButtonLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionRandomEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	for (auto &m : _entryData->_macros) {
		if (!m.get()) {
			continue;
		}
		auto name = QString::fromStdString(m->Name());
		QListWidgetItem *item = new QListWidgetItem(name, _macroList);
		item->setData(Qt::UserRole, name);
	}
	SetMacroListSize();
}

void MacroActionRandomEdit::MacroRemove(const QString &name)
{
	if (_entryData) {
		auto it = _entryData->_macros.begin();
		while (it != _entryData->_macros.end()) {
			if (it->get()->Name() == name.toStdString()) {
				it = _entryData->_macros.erase(it);
			} else {
				++it;
			}
		}
	}
}

void MacroActionRandomEdit::MacroRename(const QString &oldName,
					const QString &newName)
{
	auto count = _macroList->count();
	for (int idx = 0; idx < count; ++idx) {
		QListWidgetItem *item = _macroList->item(idx);
		QString itemString = item->data(Qt::UserRole).toString();
		if (oldName == itemString) {
			item->setData(Qt::UserRole, newName);
			item->setText(newName);
			break;
		}
	}
}

void MacroActionRandomEdit::AddMacro()
{
	if (_loading || !_entryData) {
		return;
	}

	std::string macroName;
	bool accepted = MacroSelectionDialog::AskForMacro(this, macroName);

	if (!accepted || macroName.empty()) {
		return;
	}

	MacroRef macro(macroName);

	if (!macro.get()) {
		return;
	}

	if (FindEntry(macro->Name()) != -1) {
		return;
	}

	QVariant v = QVariant::fromValue(QString::fromStdString(macroName));
	QListWidgetItem *item = new QListWidgetItem(
		QString::fromStdString(macroName), _macroList);
	item->setData(Qt::UserRole, QString::fromStdString(macroName));

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros.push_back(macro);
	SetMacroListSize();
}

void MacroActionRandomEdit::RemoveMacro()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	auto item = _macroList->currentItem();
	if (!item) {
		return;
	}
	std::string name = item->data(Qt::UserRole).toString().toStdString();
	for (auto it = _entryData->_macros.begin();
	     it != _entryData->_macros.end(); ++it) {
		auto m = *it;
		if (m.get() && m->Name() == name) {
			_entryData->_macros.erase(it);
			break;
		}
	}
	delete item;
	SetMacroListSize();
}

int MacroActionRandomEdit::FindEntry(const std::string &macro)
{
	int count = _macroList->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = _macroList->item(i);
		QString itemString = item->data(Qt::UserRole).toString();
		if (QString::fromStdString(macro) == itemString) {
			idx = i;
			break;
		}
	}

	return idx;
}

void MacroActionRandomEdit::SetMacroListSize()
{
	setHeightToContentHeight(_macroList);
	adjustSize();
}
