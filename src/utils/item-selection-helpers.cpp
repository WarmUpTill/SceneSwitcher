#include "item-selection-helpers.hpp"
#include "utility.hpp"
#include "name-dialog.hpp"

#include <algorithm>
#include <QAction>
#include <QMenu>
#include <QLayout>
#include <obs-module.h>

Q_DECLARE_METATYPE(advss::Item *);

namespace advss {

Item::Item(std::string name) : _name(name) {}

static Item *GetItemByName(const std::string &name,
			   std::deque<std::shared_ptr<Item>> &items)
{
	for (auto &item : items) {
		if (item->Name() == name) {
			return item.get();
		}
	}
	return nullptr;
}

static Item *GetItemByName(const QString &name,
			   std::deque<std::shared_ptr<Item>> &items)
{
	return GetItemByName(name.toStdString(), items);
}

static bool ItemNameAvailable(const QString &name,
			      std::deque<std::shared_ptr<Item>> &items)
{
	return !GetItemByName(name, items);
}

static bool ItemNameAvailable(const std::string &name,
			      std::deque<std::shared_ptr<Item>> &items)
{
	return ItemNameAvailable(QString::fromStdString(name), items);
}

ItemSelection::ItemSelection(std::deque<std::shared_ptr<Item>> &items,
			     CreateItemFunc create, SettingsCallback callback,
			     std::string_view select, std::string_view add,
			     std::string_view conflict,
			     std::string_view configureTooltip, QWidget *parent)
	: QWidget(parent),
	  _selection(new FilterComboBox(this, obs_module_text(select.data()))),
	  _modify(new QPushButton),
	  _create(create),
	  _askForSettings(callback),
	  _items(items),
	  _selectStr(select),
	  _addStr(add),
	  _conflictStr(conflict)
{
	_modify->setMaximumWidth(22);
	SetButtonIcon(_modify, ":/settings/images/settings/general.svg");
	_modify->setFlat(true);
	if (!configureTooltip.empty()) {
		_modify->setToolTip(obs_module_text(configureTooltip.data()));
	}

	// Connect to slots
	QWidget::connect(_selection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(ChangeSelection(const QString &)));
	QWidget::connect(_modify, SIGNAL(clicked()), this,
			 SLOT(ModifyButtonClicked()));

	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(_selection);
	layout->addWidget(_modify);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	for (const auto &i : items) {
		_selection->addItem(QString::fromStdString(i->_name));
	}
	_selection->model()->sort(0);
	_selection->insertSeparator(_selection->count());
	_selection->addItem(obs_module_text(_addStr.data()));
}

void ItemSelection::SetItem(const std::string &item)
{
	const QSignalBlocker blocker(_selection);
	if (!!GetItemByName(item, _items)) {
		_selection->setCurrentText(QString::fromStdString(item));
	} else {
		_selection->setCurrentIndex(-1);
	}
}

void ItemSelection::ShowRenameContextMenu(bool value)
{
	_showRenameContextMenu = value;
}

void ItemSelection::ChangeSelection(const QString &sel)
{
	if (sel == obs_module_text(_addStr.data())) {
		auto item = _create();
		bool accepted = _askForSettings(this, *item.get());
		if (!accepted) {
			_selection->setCurrentIndex(-1);
			return;
		}
		_items.emplace_back(item);
		const QSignalBlocker b(_selection);
		const QString name = QString::fromStdString(item->_name);
		AddItem(name);
		_selection->setCurrentText(name);
		emit ItemAdded(name);
		emit SelectionChanged(name);
		return;
	}
	auto item = GetCurrentItem();
	if (item) {
		emit SelectionChanged(QString::fromStdString(item->_name));
	} else {
		emit SelectionChanged("");
	}
}

void ItemSelection::ModifyButtonClicked()
{
	auto item = GetCurrentItem();
	if (!item) {
		return;
	}
	auto properties = [&]() {
		const auto oldName = item->_name;
		bool accepted = _askForSettings(this, *item);
		if (!accepted) {
			return;
		}
		if (oldName != item->_name) {
			emit ItemRenamed(QString::fromStdString(oldName),
					 QString::fromStdString(item->_name));
		}
	};

	QMenu menu(this);
	QAction *action;
	if (_showRenameContextMenu) {
		action = new QAction(
			obs_module_text("AdvSceneSwitcher.item.rename"), &menu);
		connect(action, SIGNAL(triggered()), this, SLOT(RenameItem()));
		action->setProperty("item", QVariant::fromValue(item));
		menu.addAction(action);
	}

	action = new QAction(obs_module_text("AdvSceneSwitcher.item.remove"),
			     &menu);
	connect(action, SIGNAL(triggered()), this, SLOT(RemoveItem()));
	menu.addAction(action);

	action = new QAction(
		obs_module_text("AdvSceneSwitcher.item.properties"), &menu);
	connect(action, &QAction::triggered, properties);
	menu.addAction(action);

	menu.exec(QCursor::pos());
}

void ItemSelection::RenameItem()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	QVariant variant = action->property("item");
	Item *item = variant.value<Item *>();

	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.windowTitle"),
		obs_module_text("AdvSceneSwitcher.item.newName"), name,
		QString::fromStdString(name));
	if (!accepted) {
		return;
	}
	if (name.empty()) {
		DisplayMessage(
			obs_module_text("AdvSceneSwitcher.item.emptyName"));
		return;
	}
	if (_selection->currentText().toStdString() != name &&
	    !ItemNameAvailable(name, _items)) {
		DisplayMessage(obs_module_text(_conflictStr.data()));
		return;
	}

	const auto oldName = item->_name;
	item->_name = name;
	emit ItemRenamed(QString::fromStdString(oldName),
			 QString::fromStdString(name));
}

void ItemSelection::RenameItem(const QString &oldName, const QString &name)
{
	int idx = _selection->findText(oldName);
	if (idx == -1) {
		return;
	}
	_selection->setItemText(idx, name);
}

void ItemSelection::AddItem(const QString &name)
{
	if (_selection->findText(name) == -1) {
		_selection->insertItem(1, name);
	}
}

void ItemSelection::RemoveItem()
{
	auto item = GetCurrentItem();
	if (!item) {
		return;
	}

	int idx = _selection->findText(QString::fromStdString(item->_name));
	if (idx == -1 || idx == _selection->count()) {
		return;
	}

	auto name = item->_name;
	for (auto it = _items.begin(); it != _items.end(); ++it) {
		if (it->get()->_name == item->_name) {
			_items.erase(it);
			break;
		}
	}

	emit ItemRemoved(QString::fromStdString(name));
}

void ItemSelection::RemoveItem(const QString &name)
{
	const int idx = _selection->findText(name);
	if (idx == _selection->currentIndex()) {
		_selection->setCurrentIndex(-1);
	}
	_selection->removeItem(idx);
}

Item *ItemSelection::GetCurrentItem()
{
	return GetItemByName(_selection->currentText(), _items);
}

ItemSettingsDialog::ItemSettingsDialog(const Item &settings,
				       std::deque<std::shared_ptr<Item>> &items,
				       std::string_view select,
				       std::string_view add,
				       std::string_view nameConflict,
				       QWidget *parent)
	: QDialog(parent),
	  _name(new QLineEdit()),
	  _nameHint(new QLabel),
	  _buttonbox(new QDialogButtonBox(QDialogButtonBox::Ok |
					  QDialogButtonBox::Cancel)),
	  _items(items),
	  _selectStr(select),
	  _addStr(add),
	  _conflictStr(nameConflict)
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setMinimumWidth(555);
	setMinimumHeight(100);

	_buttonbox->setCenterButtons(true);
	_buttonbox->button(QDialogButtonBox::Ok)->setDisabled(true);

	_originalName = QString::fromStdString(settings._name);
	_name->setText(_originalName);

	QWidget::connect(_name, SIGNAL(textEdited(const QString &)), this,
			 SLOT(NameChanged(const QString &)));
	QWidget::connect(_buttonbox, &QDialogButtonBox::accepted, this,
			 &QDialog::accept);
	QWidget::connect(_buttonbox, &QDialogButtonBox::rejected, this,
			 &QDialog::reject);

	NameChanged(_name->text());
}

void ItemSettingsDialog::NameChanged(const QString &text)
{

	if (text != _originalName && !ItemNameAvailable(text, _items)) {
		SetNameWarning(obs_module_text(_conflictStr.data()));
		return;
	}
	if (text.isEmpty()) {
		SetNameWarning(
			obs_module_text("AdvSceneSwitcher.item.emptyName"));
		return;
	}
	if (text == obs_module_text(_selectStr.data()) ||
	    text == obs_module_text(_addStr.data())) {
		SetNameWarning(
			obs_module_text("AdvSceneSwitcher.item.nameReserved"));
		return;
	}
	SetNameWarning("");
}

void ItemSettingsDialog::SetNameWarning(const QString warn)
{
	if (warn.isEmpty()) {
		_nameHint->hide();
		_buttonbox->button(QDialogButtonBox::Ok)->setDisabled(false);
		return;
	}
	_nameHint->setText(warn);
	_nameHint->show();
	_buttonbox->button(QDialogButtonBox::Ok)->setDisabled(true);
}

void Item::Load(obs_data_t *obj)
{
	_name = obs_data_get_string(obj, "name");
}

void Item::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "name", _name.c_str());
}

} // namespace advss
