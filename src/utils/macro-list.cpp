#include "macro-list.hpp"
#include "macro.hpp"
#include "macro-selection.hpp"
#include "utility.hpp"

MacroList::MacroList(QWidget *parent, bool allowDuplicates, bool reorder)
	: QWidget(parent),
	  _list(new QListWidget()),
	  _add(new QPushButton()),
	  _remove(new QPushButton()),
	  _up(new QPushButton()),
	  _down(new QPushButton()),
	  _controlsLayout(new QHBoxLayout()),
	  _allowDuplicates(allowDuplicates),
	  _reorder(reorder)
{
	_up->setVisible(reorder);
	_down->setVisible(reorder);

	_add->setMaximumWidth(22);
	_add->setProperty("themeID",
			  QVariant(QString::fromUtf8("addIconSmall")));
	_add->setFlat(true);
	_remove->setMaximumWidth(22);
	_remove->setProperty("themeID",
			     QVariant(QString::fromUtf8("removeIconSmall")));
	_remove->setFlat(true);
	_up->setMaximumWidth(22);
	_up->setProperty("themeID",
			 QVariant(QString::fromUtf8("upArrowIconSmall")));
	_up->setFlat(true);
	_down->setMaximumWidth(22);
	_down->setProperty("themeID",
			   QVariant(QString::fromUtf8("downArrowIconSmall")));
	_down->setFlat(true);

	QWidget::connect(_add, SIGNAL(clicked()), this, SLOT(Add()));
	QWidget::connect(_remove, SIGNAL(clicked()), this, SLOT(Remove()));
	QWidget::connect(_up, SIGNAL(clicked()), this, SLOT(Up()));
	QWidget::connect(_down, SIGNAL(clicked()), this, SLOT(Down()));
	QWidget::connect(_list, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
			 this, SLOT(MacroItemClicked(QListWidgetItem *)));
	QWidget::connect(window(),
			 SIGNAL(MacroRenamed(const QString &, const QString &)),
			 this,
			 SLOT(MacroRename(const QString &, const QString &)));
	QWidget::connect(window(), SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));

	_controlsLayout->addWidget(_add);
	_controlsLayout->addWidget(_remove);
	if (reorder) {
		QFrame *line = new QFrame();
		line->setFrameShape(QFrame::VLine);
		line->setFrameShadow(QFrame::Sunken);
		_controlsLayout->addWidget(line);
	}
	_controlsLayout->addWidget(_up);
	_controlsLayout->addWidget(_down);
	_controlsLayout->addStretch();

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_list);
	mainLayout->addLayout(_controlsLayout);
	setLayout(mainLayout);
	SetMacroListSize();
}

void MacroList::SetContent(const std::vector<MacroRef> &macros)
{
	for (auto &m : macros) {
		QString name;
		if (!m.get()) {
			name = QString::fromStdString(m.RefName()) + "<" +
			       obs_module_text(
				       "AdvSceneSwitcher.macroList.deleted") +
			       ">";
		} else {
			name = QString::fromStdString(m->Name());
		}
		QListWidgetItem *item = new QListWidgetItem(name, _list);
		item->setData(Qt::UserRole, name);
	}
	SetMacroListSize();
}

void MacroList::AddControl(QWidget *widget)
{
	_controlsLayout->insertWidget(_controlsLayout->count() - 1, widget);
}

int MacroList::CurrentRow()
{
	return _list->currentRow();
}

void MacroList::MacroRename(const QString &oldName, const QString &newName)
{
	auto count = _list->count();
	for (int idx = 0; idx < count; ++idx) {
		QListWidgetItem *item = _list->item(idx);
		QString itemString = item->data(Qt::UserRole).toString();
		if (oldName == itemString) {
			item->setData(Qt::UserRole, newName);
			item->setText(newName);
		}
	}
}

void MacroList::MacroRemove(const QString &name)
{
	int idx = FindEntry(name.toStdString());
	while (idx != -1) {
		delete _list->item(idx);
		idx = FindEntry(name.toStdString());
	}
	SetMacroListSize();
}

void MacroList::Add()
{
	std::string macroName;
	bool accepted = MacroSelectionDialog::AskForMacro(this, macroName);

	if (!accepted || macroName.empty()) {
		return;
	}

	if (!_allowDuplicates && FindEntry(macroName) != -1) {
		return;
	}

	QVariant v = QVariant::fromValue(QString::fromStdString(macroName));
	auto item =
		new QListWidgetItem(QString::fromStdString(macroName), _list);
	item->setData(Qt::UserRole, QString::fromStdString(macroName));
	SetMacroListSize();
	emit Added(macroName);
}

void MacroList::Remove()
{
	auto item = _list->currentItem();
	int idx = _list->currentRow();
	if (!item || idx == -1) {
		return;
	}
	delete item;
	SetMacroListSize();
	emit Removed(idx);
}

void MacroList::Up()
{
	int idx = _list->currentRow();
	if (idx != -1 && idx != 0) {
		_list->insertItem(idx - 1, _list->takeItem(idx));
		_list->setCurrentRow(idx - 1);
		emit MovedUp(idx);
	}
}

void MacroList::Down()
{
	int idx = _list->currentRow();
	if (idx != -1 && idx != _list->count() - 1) {
		_list->insertItem(idx + 1, _list->takeItem(idx));
		_list->setCurrentRow(idx + 1);
		emit MovedDown(idx);
	}
}

void MacroList::MacroItemClicked(QListWidgetItem *item)
{
	std::string macroName;
	bool accepted = MacroSelectionDialog::AskForMacro(this, macroName);

	if (!accepted || macroName.empty()) {
		return;
	}

	if (!_allowDuplicates && FindEntry(macroName) != -1) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroList.duplicate");
		DisplayMessage(err.arg(QString::fromStdString(macroName)));
		return;
	}

	item->setText(QString::fromStdString(macroName));
	int idx = _list->currentRow();
	emit Replaced(idx, macroName);
}

int MacroList::FindEntry(const std::string &macro)
{
	int count = _list->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = _list->item(i);
		QString itemString = item->data(Qt::UserRole).toString();
		if (QString::fromStdString(macro) == itemString) {
			idx = i;
			break;
		}
	}

	return idx;
}

void MacroList::SetMacroListSize()
{
	setHeightToContentHeight(_list);
	adjustSize();
}
