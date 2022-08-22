#include "macro-action-random.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

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
		if (!macros[0].get() || macros[0]->Paused()) {
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
	if (macros.size() == 1 && macros[0].get()) {
		lastRandomMacro = macros[0];
		return macros[0]->PerformActions();
	}
	srand((unsigned int)time(0));
	size_t idx = std::rand() % (macros.size());
	lastRandomMacro = macros[idx];
	return macros[idx]->PerformActions();
}

void MacroActionRandom::LogAction()
{
	vblog(LOG_INFO, "running random macro");
}

bool MacroActionRandom::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	SaveMacroList(obj, _macros);
	return true;
}

bool MacroActionRandom::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	LoadMacroList(obj, _macros);
	return true;
}

MacroActionRandomEdit::MacroActionRandomEdit(
	QWidget *parent, std::shared_ptr<MacroActionRandom> entryData)
	: QWidget(parent), _list(new MacroList(this, false, false))
{
	QWidget::connect(_list, SIGNAL(Added(const std::string &)), this,
			 SLOT(Add(const std::string &)));
	QWidget::connect(_list, SIGNAL(Removed(int)), this, SLOT(Remove(int)));
	QWidget::connect(_list, SIGNAL(Replaced(int, const std::string &)),
			 this, SLOT(Replace(int, const std::string &)));
	QWidget::connect(window(), SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.random.entry"),
		     entryLayout, widgetPlaceholders);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_list);
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

	_list->SetContent(_entryData->_macros);
	adjustSize();
}

void MacroActionRandomEdit::MacroRemove(const QString &)
{
	if (!_entryData) {
		return;
	}

	auto it = _entryData->_macros.begin();
	while (it != _entryData->_macros.end()) {
		it->UpdateRef();
		if (it->get() == nullptr) {
			it = _entryData->_macros.erase(it);
		} else {
			++it;
		}
	}
	adjustSize();
}

void MacroActionRandomEdit::Add(const std::string &name)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	MacroRef macro(name);
	_entryData->_macros.push_back(macro);
	adjustSize();
}

void MacroActionRandomEdit::Remove(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));
	adjustSize();
}

void MacroActionRandomEdit::Replace(int idx, const std::string &name)
{
	if (_loading || !_entryData) {
		return;
	}

	MacroRef macro(name);
	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros[idx] = macro;
	adjustSize();
}
