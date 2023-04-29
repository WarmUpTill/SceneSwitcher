#include "macro-action-sequence.hpp"
#include "macro.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroActionSequence::id = "sequence";

bool MacroActionSequence::_registered = MacroActionFactory::Register(
	MacroActionSequence::id,
	{MacroActionSequence::Create, MacroActionSequenceEdit::Create,
	 "AdvSceneSwitcher.action.sequence"});

int getNextUnpausedMacroIdx(std::vector<MacroRef> &macros, int startIdx)
{
	for (; (int)macros.size() > startIdx; ++startIdx) {
		auto macro = macros[startIdx].GetMacro();
		if (macro && !macro->Paused()) {
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

bool MacroActionSequence::PerformAction()
{
	if (_macros.size() == 0) {
		return true;
	}

	auto macro = GetNextMacro().GetMacro();
	if (!macro.get()) {
		return true;
	}

	return macro->PerformActions();
}

void MacroActionSequence::LogAction() const
{
	vblog(LOG_INFO, "running macro sequence");
}

bool MacroActionSequence::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	SaveMacroList(obj, _macros);
	obs_data_set_bool(obj, "restart", _restart);
	return true;
}

bool MacroActionSequence::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	LoadMacroList(obj, _macros);
	_restart = obs_data_get_bool(obj, "restart");
	return true;
}

MacroActionSequenceEdit::MacroActionSequenceEdit(
	QWidget *parent, std::shared_ptr<MacroActionSequence> entryData)
	: QWidget(parent),
	  _list(new MacroList(this, true, true)),
	  _continueFrom(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.action.sequence.continueFrom"))),
	  _restart(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.sequence.restart"))),
	  _statusLine(new QLabel())
{
	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Sunken);
	_list->AddControl(line);
	_list->AddControl(_continueFrom);

	QWidget::connect(_list, SIGNAL(Added(const std::string &)), this,
			 SLOT(Add(const std::string &)));
	QWidget::connect(_list, SIGNAL(Removed(int)), this, SLOT(Remove(int)));
	QWidget::connect(_list, SIGNAL(MovedUp(int)), this, SLOT(Up(int)));
	QWidget::connect(_list, SIGNAL(MovedDown(int)), this, SLOT(Down(int)));
	QWidget::connect(_list, SIGNAL(Replaced(int, const std::string &)),
			 this, SLOT(Replace(int, const std::string &)));
	QWidget::connect(_continueFrom, SIGNAL(clicked()), this,
			 SLOT(ContinueFromClicked()));
	QWidget::connect(_restart, SIGNAL(stateChanged(int)), this,
			 SLOT(RestartChanged(int)));
	QWidget::connect(window(), SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.sequence.entry"),
		     entryLayout, widgetPlaceholders);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_list);
	mainLayout->addWidget(_restart);
	mainLayout->addWidget(_statusLine);
	setLayout(mainLayout);

	UpdateStatusLine();
	connect(&_statusTimer, SIGNAL(timeout()), this,
		SLOT(UpdateStatusLine()));
	_statusTimer.start(1000);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSequenceEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_list->SetContent(_entryData->_macros);
	_restart->setChecked(_entryData->_restart);
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
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	MacroRef macro(name);
	_entryData->_macros.push_back(macro);
	adjustSize();
}

void MacroActionSequenceEdit::Remove(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));
	adjustSize();
}

void MacroActionSequenceEdit::Up(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	std::swap(_entryData->_macros[idx], _entryData->_macros[idx - 1]);
}

void MacroActionSequenceEdit::Down(int idx)
{
	auto lock = LockContext();
	std::swap(_entryData->_macros[idx], _entryData->_macros[idx + 1]);
}

void MacroActionSequenceEdit::Replace(int idx, const std::string &name)
{
	if (_loading || !_entryData) {
		return;
	}

	MacroRef macro(name);
	auto lock = LockContext();
	_entryData->_macros[idx] = macro;
	adjustSize();
}

void MacroActionSequenceEdit::ContinueFromClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	int idx = _list->CurrentRow();
	if (idx == -1) {
		return;
	}
	auto lock = LockContext();
	_entryData->_lastIdx = idx - 1;
}

void MacroActionSequenceEdit::RestartChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
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
			lastMacroName = QString::fromStdString(macro->Name());
		}
		auto next = _entryData->GetNextMacro(false).GetMacro();
		if (next) {
			nextMacroName = QString::fromStdString(next->Name());
		}
	}

	QString format{
		obs_module_text("AdvSceneSwitcher.action.sequence.status")};
	_statusLine->setText(format.arg(lastMacroName, nextMacroName));
}

} // namespace advss
