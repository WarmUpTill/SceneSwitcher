#include "macro-action-random.hpp"
#include "macro-helpers.hpp"
#include "utility.hpp"

#include <cstdlib>

namespace advss {

const std::string MacroActionRandom::id = "random";

bool MacroActionRandom::_registered = MacroActionFactory::Register(
	MacroActionRandom::id,
	{MacroActionRandom::Create, MacroActionRandomEdit::Create,
	 "AdvSceneSwitcher.action.random"});

static bool validNextMacro(const std::shared_ptr<Macro> &macro)
{
	return !MacroIsPaused(macro.get());
}

static std::vector<std::shared_ptr<Macro>>
getNextMacros(std::vector<MacroRef> &macros, MacroRef &lastRandomMacroRef,
	      bool allowRepeat)
{
	std::vector<std::shared_ptr<Macro>> res;
	if (macros.size() == 1) {
		auto macro = macros[0].GetMacro();
		if (validNextMacro(macro)) {
			res.push_back(macro);
		}
		return res;
	}

	auto lastRandomMacro = lastRandomMacroRef.GetMacro();
	for (auto &m : macros) {
		auto macro = m.GetMacro();
		if (validNextMacro(macro) &&
		    (allowRepeat || (lastRandomMacro != macro))) {
			res.push_back(macro);
		}
	}
	return res;
}

bool MacroActionRandom::PerformAction()
{
	if (_macros.size() == 0) {
		return true;
	}

	auto macros = getNextMacros(_macros, lastRandomMacro, _allowRepeat);
	if (macros.size() == 0) {
		return true;
	}
	if (macros.size() == 1) {
		lastRandomMacro = macros[0];
		return RunMacroActions(macros[0].get());
	}
	srand((unsigned int)time(0));
	size_t idx = std::rand() % (macros.size());
	lastRandomMacro = macros[idx];
	return RunMacroActions(macros[idx].get());
}

void MacroActionRandom::LogAction() const
{
	vblog(LOG_INFO, "running random macro");
}

bool MacroActionRandom::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	SaveMacroList(obj, _macros);
	obs_data_set_bool(obj, "allowRepeat", _allowRepeat);
	return true;
}

bool MacroActionRandom::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	LoadMacroList(obj, _macros);
	_allowRepeat = obs_data_get_bool(obj, "allowRepeat");
	return true;
}

MacroActionRandomEdit::MacroActionRandomEdit(
	QWidget *parent, std::shared_ptr<MacroActionRandom> entryData)
	: QWidget(parent),
	  _list(new MacroList(this, true, false)),
	  _allowRepeat(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.action.random.allowRepeat")))
{
	QWidget::connect(_list, SIGNAL(Added(const std::string &)), this,
			 SLOT(Add(const std::string &)));
	QWidget::connect(_list, SIGNAL(Removed(int)), this, SLOT(Remove(int)));
	QWidget::connect(_list, SIGNAL(Replaced(int, const std::string &)),
			 this, SLOT(Replace(int, const std::string &)));
	QWidget::connect(window(), SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_allowRepeat, SIGNAL(stateChanged(int)), this,
			 SLOT(AllowRepeatChanged(int)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.random.entry"),
		     entryLayout, widgetPlaceholders);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_list);
	mainLayout->addWidget(_allowRepeat);
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
	_allowRepeat->setChecked(_entryData->_allowRepeat);
	_allowRepeat->setVisible(ShouldShowAllowRepeat());
	adjustSize();
}

void MacroActionRandomEdit::MacroRemove(const QString &)
{
	if (!_entryData) {
		return;
	}

	auto it = _entryData->_macros.begin();
	while (it != _entryData->_macros.end()) {
		if (!it->GetMacro()) {
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

	auto lock = LockContext();
	MacroRef macro(name);
	_entryData->_macros.push_back(macro);
	_allowRepeat->setVisible(ShouldShowAllowRepeat());
	adjustSize();
}

void MacroActionRandomEdit::Remove(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));
	_allowRepeat->setVisible(ShouldShowAllowRepeat());
	adjustSize();
}

void MacroActionRandomEdit::Replace(int idx, const std::string &name)
{
	if (_loading || !_entryData) {
		return;
	}

	MacroRef macro(name);
	auto lock = LockContext();
	_entryData->_macros[idx] = macro;
	adjustSize();
}

void MacroActionRandomEdit::AllowRepeatChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_allowRepeat = value;
}

bool MacroActionRandomEdit::ShouldShowAllowRepeat()
{
	if (_entryData->_macros.size() <= 1) {
		return false;
	}
	const auto macro = _entryData->_macros[0].GetMacro();
	for (const auto &m : _entryData->_macros) {
		if (macro != m.GetMacro()) {
			return true;
		}
	}
	return false;
}

} // namespace advss
