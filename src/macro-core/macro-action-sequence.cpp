#include "macro-action-sequence.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionSequence::id = "sequence";

bool MacroActionSequence::_registered = MacroActionFactory::Register(
	MacroActionSequence::id,
	{MacroActionSequence::Create, MacroActionSequenceEdit::Create,
	 "AdvSceneSwitcher.action.sequence"});

int getNextUnpausedMacroIdx(std::vector<MacroRef> &macros, int startIdx)
{
	for (; (int)macros.size() > startIdx; ++startIdx) {
		if (macros[startIdx].get() && !macros[startIdx]->Paused()) {
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

	auto macro = GetNextMacro();
	if (!macro.get()) {
		return true;
	}

	return macro->PerformActions();
}

void MacroActionSequence::LogAction()
{
	vblog(LOG_INFO, "running macro sequence");
}

bool MacroActionSequence::Save(obs_data_t *obj)
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
	: QWidget(parent)
{
	_macroList = new QListWidget();
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
	_up = new QPushButton();
	_up->setMaximumSize(QSize(22, 22));
	_up->setProperty("themeID",
			 QVariant(QString::fromUtf8("upArrowIconSmall")));
	_up->setFlat(true);
	_down = new QPushButton();
	_down->setMaximumSize(QSize(22, 22));
	_down->setProperty("themeID",
			   QVariant(QString::fromUtf8("downArrowIconSmall")));
	_down->setFlat(true);
	_continueFrom = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.action.sequence.continueFrom"));
	_restart = new QCheckBox(
		obs_module_text("AdvSceneSwitcher.action.sequence.restart"));
	_statusLine = new QLabel();

	QWidget::connect(_add, SIGNAL(clicked()), this, SLOT(Add()));
	QWidget::connect(_remove, SIGNAL(clicked()), this, SLOT(Remove()));
	QWidget::connect(_up, SIGNAL(clicked()), this, SLOT(Up()));
	QWidget::connect(_down, SIGNAL(clicked()), this, SLOT(Down()));
	QWidget::connect(_continueFrom, SIGNAL(clicked()), this,
			 SLOT(ContinueFromClicked()));
	QWidget::connect(_macroList,
			 SIGNAL(itemDoubleClicked(QListWidgetItem *)), this,
			 SLOT(MacroItemClicked(QListWidgetItem *)));

	QWidget::connect(window(),
			 SIGNAL(MacroRenamed(const QString &, const QString &)),
			 this,
			 SLOT(MacroRename(const QString &, const QString &)));
	QWidget::connect(_restart, SIGNAL(stateChanged(int)), this,
			 SLOT(RestartChanged(int)));

	auto *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.sequence.entry"),
		     entryLayout, widgetPlaceholders);

	auto *argButtonLayout = new QHBoxLayout;
	argButtonLayout->addWidget(_add);
	argButtonLayout->addWidget(_remove);
	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Sunken);
	argButtonLayout->addWidget(line);
	argButtonLayout->addWidget(_up);
	argButtonLayout->addWidget(_down);
	QFrame *line2 = new QFrame();
	line2->setFrameShape(QFrame::VLine);
	line2->setFrameShadow(QFrame::Sunken);
	argButtonLayout->addWidget(line2);
	argButtonLayout->addWidget(_continueFrom);
	argButtonLayout->addStretch();

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_macroList);
	mainLayout->addLayout(argButtonLayout);
	mainLayout->addLayout(argButtonLayout);
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

	for (auto &m : _entryData->_macros) {
		if (!m.get()) {
			continue;
		}
		auto name = QString::fromStdString(m->Name());
		QListWidgetItem *item = new QListWidgetItem(name, _macroList);
		item->setData(Qt::UserRole, name);
	}
	SetMacroListSize();
	_restart->setChecked(_entryData->_restart);
}

void MacroActionSequenceEdit::MacroRemove(const QString &name)
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

void MacroActionSequenceEdit::MacroRename(const QString &oldName,
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

void MacroActionSequenceEdit::Add()
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

	QVariant v = QVariant::fromValue(QString::fromStdString(macroName));
	new QListWidgetItem(QString::fromStdString(macroName), _macroList);
	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros.push_back(macro);
	SetMacroListSize();
}

void MacroActionSequenceEdit::Remove()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	auto item = _macroList->currentItem();
	int idx = _macroList->currentRow();
	if (!item || idx == -1) {
		return;
	}

	_entryData->_macros.erase(std::next(_entryData->_macros.begin(), idx));

	delete item;
	SetMacroListSize();
}

void MacroActionSequenceEdit::Up()
{
	if (_loading || !_entryData) {
		return;
	}

	int idx = _macroList->currentRow();
	if (idx != -1 && idx != 0) {
		_macroList->insertItem(idx - 1, _macroList->takeItem(idx));
		_macroList->setCurrentRow(idx - 1);

		std::lock_guard<std::mutex> lock(switcher->m);
		std::swap(_entryData->_macros[idx],
			  _entryData->_macros[idx - 1]);
	}
}

void MacroActionSequenceEdit::Down()
{
	int idx = _macroList->currentRow();
	if (idx != -1 && idx != _macroList->count() - 1) {
		_macroList->insertItem(idx + 1, _macroList->takeItem(idx));
		_macroList->setCurrentRow(idx + 1);

		std::lock_guard<std::mutex> lock(switcher->m);
		std::swap(_entryData->_macros[idx],
			  _entryData->_macros[idx + 1]);
	}
}

void MacroActionSequenceEdit::MacroItemClicked(QListWidgetItem *item)
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

	item->setText(QString::fromStdString(macroName));
	int idx = _macroList->currentRow();
	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macros[idx] = macro;
	SetMacroListSize();
}

void MacroActionSequenceEdit::ContinueFromClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	int idx = _macroList->currentRow();
	if (idx == -1) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_lastIdx = idx - 1;
}

void MacroActionSequenceEdit::RestartChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_restart = state;
}

void MacroActionSequenceEdit::UpdateStatusLine()
{
	QString lastMacroName =
		obs_module_text("AdvSceneSwitcher.action.sequence.status.none");
	QString nextMacroName =
		obs_module_text("AdvSceneSwitcher.action.sequence.status.none");
	if (_entryData) {
		if (_entryData->_lastSequenceMacro.get()) {
			lastMacroName = QString::fromStdString(
				_entryData->_lastSequenceMacro->Name());
		}
		auto next = _entryData->GetNextMacro(false);
		if (next.get()) {
			nextMacroName = QString::fromStdString(next->Name());
		}
	}

	QString format{
		obs_module_text("AdvSceneSwitcher.action.sequence.status")};
	_statusLine->setText(format.arg(lastMacroName, nextMacroName));
}

int MacroActionSequenceEdit::FindEntry(const std::string &macro)
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

void MacroActionSequenceEdit::SetMacroListSize()
{
	setHeightToContentHeight(_macroList);
	adjustSize();
}
