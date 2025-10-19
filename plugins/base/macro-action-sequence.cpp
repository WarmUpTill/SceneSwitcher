#include "macro-action-sequence.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "macro-signals.hpp"

namespace advss {

const std::string MacroActionSequence::id = "sequence";

bool MacroActionSequence::_registered = MacroActionFactory::Register(
	MacroActionSequence::id,
	{MacroActionSequence::Create, MacroActionSequenceEdit::Create,
	 "AdvSceneSwitcher.action.sequence"});

static const std::map<MacroActionSequence::Action, std::string> actionTypes = {
	{MacroActionSequence::Action::RUN_SEQUENCE,
	 "AdvSceneSwitcher.action.sequence.action.run"},
	{MacroActionSequence::Action::SET_INDEX,
	 "AdvSceneSwitcher.action.sequence.action.setIndex"},
};

static int getNextUnpausedMacroIdx(std::vector<MacroRef> &macros, int startIdx)
{
	if (startIdx < 0) {
		return -1;
	}

	for (; (int)macros.size() > startIdx; ++startIdx) {
		auto macro = macros[startIdx].GetMacro();
		if (!MacroIsPaused(macro.get())) {
			return startIdx;
		}
	}
	return -1;
}

MacroRef MacroActionSequence::GetNextMacro(bool advance)
{
	int idx = _lastIdx;
	MacroRef res;

	int nextUnpausedIdx = getNextUnpausedMacroIdx(_macros, idx + 1);
	if (nextUnpausedIdx != -1) {
		res = _macros[nextUnpausedIdx];
		idx = nextUnpausedIdx;
	} else {
		if (_restart) {
			idx = getNextUnpausedMacroIdx(_macros, 0);
			if (idx != -1) {
				res = _macros[idx];
			} else {
				// End of sequence reached, as all available
				// macros are either paused or the macro list
				// is empty
				idx = _macros.size();
			}
		} else {
			// End of sequence reached
			idx = _macros.size();
		}
	}

	if (advance) {
		_lastIdx = idx;
		_lastSequenceMacro = res;
	}
	return res;
}

std::shared_ptr<MacroAction> MacroActionSequence::Create(Macro *m)
{
	return std::make_shared<MacroActionSequence>(m);
}

std::shared_ptr<MacroAction> MacroActionSequence::Copy() const
{
	return std::make_shared<MacroActionSequence>(*this);
}

void MacroActionSequence::ResolveVariablesToFixedValues()
{
	_resetIndex.ResolveVariables();
}

bool MacroActionSequence::RunSequence()
{
	if (_macros.size() == 0) {
		return true;
	}

	auto macro = GetNextMacro().GetMacro();
	if (!macro.get()) {
		return true;
	}

	return RunMacroActions(macro.get());
}

bool MacroActionSequence::SetSequenceIndex() const
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return true;
	}

	auto actions = GetMacroActions(macro.get());
	if (!actions) {
		return true;
	}

	for (const auto &action : *actions) {
		if (action->GetId() != id) {
			continue;
		}

		auto sequenceAction =
			dynamic_cast<MacroActionSequence *>(action.get());
		if (!sequenceAction) {
			continue;
		}
		// -2 is needed since the _lastIndex starts at -1 and the reset
		// index starts at 1
		sequenceAction->_lastIdx = _resetIndex - 2;
	}
	return true;
}

bool MacroActionSequence::PerformAction()
{
	if (_action == Action::RUN_SEQUENCE) {
		return RunSequence();
	} else {
		return SetSequenceIndex();
	}
	return false;
}

void MacroActionSequence::LogAction() const
{
	if (_action == Action::RUN_SEQUENCE) {
		ablog(LOG_INFO, "running macro sequence");
	} else {
		vblog(LOG_INFO, "set sequences of macro '%s' to index %d",
		      _macro.Name().c_str(), _resetIndex.GetValue());
	}
}

bool MacroActionSequence::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	SaveMacroList(obj, _macros);
	obs_data_set_bool(obj, "restart", _restart);
	_macro.Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_resetIndex.Save(obj, "resetIndex");
	return true;
}

bool MacroActionSequence::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	LoadMacroList(obj, _macros);
	_restart = obs_data_get_bool(obj, "restart");
	_macro.Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_resetIndex.Load(obj, "resetIndex");
	return true;
}

bool MacroActionSequence::PostLoad()
{
	return MacroAction::PostLoad() && MacroRefAction::PostLoad() &&
	       MultiMacroRefAction::PostLoad();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionSequenceEdit::MacroActionSequenceEdit(
	QWidget *parent, std::shared_ptr<MacroActionSequence> entryData)
	: QWidget(parent),
	  _macroList(new MacroList(this, true, true)),
	  _continueFrom(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.sequence.continueFrom"))),
	  _restart(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.sequence.restart"))),
	  _statusLine(new QLabel()),
	  _actions(new QComboBox()),
	  _macros(new MacroSelection(this)),
	  _resetIndex(new VariableSpinBox()),
	  _layout(new QHBoxLayout())
{
	populateActionSelection(_actions);

	_resetIndex->setMinimum(1);

	(void)_macroList->AddControl(_continueFrom);

	QWidget::connect(_macroList, SIGNAL(Added(const std::string &)), this,
			 SLOT(Add(const std::string &)));
	QWidget::connect(_macroList, SIGNAL(Removed(int)), this,
			 SLOT(Remove(int)));
	QWidget::connect(_macroList, SIGNAL(MovedUp(int)), this, SLOT(Up(int)));
	QWidget::connect(_macroList, SIGNAL(MovedDown(int)), this,
			 SLOT(Down(int)));
	QWidget::connect(_macroList, SIGNAL(Replaced(int, const std::string &)),
			 this, SLOT(Replace(int, const std::string &)));
	QWidget::connect(_continueFrom, SIGNAL(clicked()), this,
			 SLOT(ContinueFromClicked()));
	QWidget::connect(_restart, SIGNAL(stateChanged(int)), this,
			 SLOT(RestartChanged(int)));
	QWidget::connect(MacroSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(
		_resetIndex,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(ResetIndexChanged(const NumberVariable<int> &)));

	_layout->addWidget(_actions);
	_layout->addWidget(_macros);
	_layout->addWidget(_resetIndex);

	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_layout);
	mainLayout->addWidget(_macroList);
	mainLayout->addWidget(_restart);
	mainLayout->addWidget(_statusLine);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	UpdateStatusLine();
	connect(&_statusTimer, SIGNAL(timeout()), this,
		SLOT(UpdateStatusLine()));
	_statusTimer.start(1000);
}

void MacroActionSequenceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_macroList->SetContent(_entryData->_macros);
	_restart->setChecked(_entryData->_restart);
	_resetIndex->SetValue(_entryData->_resetIndex);
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_macros->SetCurrentMacro(_entryData->_macro);
	SetWidgetVisibility();
	adjustSize();
}

void MacroActionSequenceEdit::MacroRemove(const QString &)
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

void MacroActionSequenceEdit::Add(const std::string &name)
{
	GUARD_LOADING_AND_LOCK();
	MacroRef macro(name);
	_entryData->_macros.push_back(macro);
	adjustSize();
}

void MacroActionSequenceEdit::Remove(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));
	adjustSize();
}

void MacroActionSequenceEdit::Up(int idx)
{
	GUARD_LOADING_AND_LOCK();
	std::swap(_entryData->_macros[idx], _entryData->_macros[idx - 1]);
}

void MacroActionSequenceEdit::Down(int idx)
{
	GUARD_LOADING_AND_LOCK();
	std::swap(_entryData->_macros[idx], _entryData->_macros[idx + 1]);
}

void MacroActionSequenceEdit::Replace(int idx, const std::string &name)
{
	GUARD_LOADING_AND_LOCK();
	MacroRef macro(name);
	_entryData->_macros[idx] = macro;
	adjustSize();
}

void MacroActionSequenceEdit::ContinueFromClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	int idx = _macroList->CurrentRow();
	if (idx == -1) {
		return;
	}
	auto lock = LockContext();
	_entryData->_lastIdx = idx - 1;
}

void MacroActionSequenceEdit::RestartChanged(int state)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_restart = state;
}

void MacroActionSequenceEdit::UpdateStatusLine()
{
	QString lastMacroName =
		obs_module_text("AdvSceneSwitcher.action.sequence.status.none");
	QString nextMacroName =
		obs_module_text("AdvSceneSwitcher.action.sequence.status.none");
	if (_entryData) {
		if (auto macro = _entryData->_lastSequenceMacro.GetMacro()) {
			lastMacroName = QString::fromStdString(
				GetMacroName(macro.get()));
		}
		auto next = _entryData->GetNextMacro(false).GetMacro();
		if (next) {
			nextMacroName = QString::fromStdString(
				GetMacroName(next.get()));
		}
	}

	QString format{
		obs_module_text("AdvSceneSwitcher.action.sequence.status")};
	_statusLine->setText(format.arg(lastMacroName, nextMacroName));
}

void MacroActionSequenceEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSequence::Action>(value);
	SetWidgetVisibility();
}

void MacroActionSequenceEdit::MacroChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_macro = text;
}

void MacroActionSequenceEdit::ResetIndexChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_resetIndex = value;
}

void MacroActionSequenceEdit::SetWidgetVisibility()
{
	_layout->removeWidget(_actions);
	_layout->removeWidget(_macros);
	_layout->removeWidget(_resetIndex);

	ClearLayout(_layout);

	PlaceWidgets(
		obs_module_text(
			_entryData->_action ==
					MacroActionSequence::Action::RUN_SEQUENCE
				? "AdvSceneSwitcher.action.sequence.entry.run"
				: "AdvSceneSwitcher.action.sequence.entry.setIndex"),
		_layout,
		{{"{{actions}}", _actions},
		 {"{{macros}}", _macros},
		 {"{{index}}", _resetIndex}});

	_macroList->setVisible(_entryData->_action ==
			       MacroActionSequence::Action::RUN_SEQUENCE);
	_restart->setVisible(_entryData->_action ==
			     MacroActionSequence::Action::RUN_SEQUENCE);
	_statusLine->setVisible(_entryData->_action ==
				MacroActionSequence::Action::RUN_SEQUENCE);
	_macros->setVisible(_entryData->_action ==
			    MacroActionSequence::Action::SET_INDEX);
	_resetIndex->setVisible(_entryData->_action ==
				MacroActionSequence::Action::SET_INDEX);

	adjustSize();
	updateGeometry();
}

} // namespace advss
