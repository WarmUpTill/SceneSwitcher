#include "key-value-list.hpp"
#include "name-dialog.hpp"
#include "ui-helpers.hpp"

#include <QLayout>
#include <QTimer>

namespace advss {

KeyValueListEdit::KeyValueListEdit(QWidget *parent, const QString &addKeyString,
				   const QString &addKeyStringDescription,
				   const QString &addValueString,
				   const QString &addValueStringDescription)
	: ListEditor(parent),
	  _addKeyString(addKeyString),
	  _addKeyStringDescription(addKeyStringDescription),
	  _addValueString(addValueString),
	  _addValueStringDescription(addValueStringDescription)
{
	_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_list->setAutoScroll(false);
}

void KeyValueListEdit::SetStringList(const StringList &list)
{
	_stringList = list;
	_list->clear();

	for (int i = 0; i < list.size(); i += 2) {
		AppendListEntryWidget(
			list.at(i), i + 1 >= list.size() ? "" : list.at(i + 1));
	}
	UpdateListSize();
}

void KeyValueListEdit::Add()
{
	StringVariable key;
	StringVariable value;
	bool accepted = AskForKeyValue(key, value);
	if (!accepted) {
		return;
	}

	AppendListEntryWidget(key, value);
	_stringList << key << value;

	UpdateListSize();

	// Delay resizing to make sure the list viewport was already updated
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });

	StringListChanged(_stringList);
}

void KeyValueListEdit::Remove()
{
	int idx = _list->currentRow();
	if (idx == -1) {
		return;
	}
	_stringList.removeAt(idx);

	QListWidgetItem *item = _list->currentItem();
	if (!item) {
		return;
	}
	delete item;

	UpdateListSize();

	// Delay resizing to make sure the list viewport was already updated
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });

	StringListChanged(_stringList);
}

void KeyValueListEdit::Up()
{
	int idx = _list->currentRow();
	if (idx <= 0 || idx >= _list->count()) {
		return;
	}

	MoveStringListIdxUp(idx);

	auto row = _list->itemWidget(_list->currentItem());
	auto newItem = _list->currentItem()->clone();

	_list->insertItem(idx - 1, newItem);
	_list->setItemWidget(newItem, row);

	_list->takeItem(idx + 1);
	_list->setCurrentRow(idx - 1);

	UpdateListSize();

	StringListChanged(_stringList);
}

void KeyValueListEdit::Down()
{
	int idx = _list->currentRow();
	if (idx == -1 || idx == _list->count() - 1) {
		return;
	}

	MoveStringListIdxUp(idx + 1);

	auto row = _list->itemWidget(_list->currentItem());
	auto newItem = _list->currentItem()->clone();

	_list->insertItem(idx + 2, newItem);
	_list->setItemWidget(newItem, row);

	_list->takeItem(idx);
	_list->setCurrentRow(idx + 1);

	UpdateListSize();

	StringListChanged(_stringList);
}

void KeyValueListEdit::Clicked(QListWidgetItem *item)
{
	int idx = _list->currentRow();

	StringVariable key = _stringList[idx * 2];
	StringVariable value = _stringList[idx * 2 + 1];
	bool accepted = AskForKeyValue(key, value);
	if (!accepted) {
		return;
	}

	auto container = static_cast<KeyValueListContainerWidget *>(
		_list->itemWidget(item));
	container->_key->setText(QString::fromStdString(key.UnresolvedValue()));
	container->_value->setText(
		QString::fromStdString(value.UnresolvedValue()));
	container->adjustSize();
	container->updateGeometry();
	UpdateListSize();

	_stringList[idx * 2] = key;
	_stringList[idx * 2 + 1] = value;

	// Delay resizing to make sure the list viewport was already updated
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });
	StringListChanged(_stringList);
}

void KeyValueListEdit::MoveStringListIdxUp(int idx)
{
	if (idx <= 0 || idx >= _list->count()) {
		return;
	}

	_stringList.move(idx * 2, idx * 2 - 2);
	_stringList.move(idx * 2 + 1, idx * 2 - 1);
}

bool KeyValueListEdit::AskForKeyValue(StringVariable &keyVariable,
				      StringVariable &valueVariable)
{
	std::string key;
	bool accepted = NameDialog::AskForName(
		this, _addKeyString, _addKeyStringDescription, key,
		QString::fromStdString(keyVariable.UnresolvedValue()), 4096,
		false);
	if (!accepted) {
		return false;
	}

	std::string value;
	accepted = NameDialog::AskForName(
		this, _addValueString, _addValueStringDescription, value,
		QString::fromStdString(valueVariable.UnresolvedValue()), 4096,
		false);
	if (!accepted) {
		return false;
	}

	keyVariable = key;
	valueVariable = value;

	return true;
}

void KeyValueListEdit::AppendListEntryWidget(const StringVariable &key,
					     const StringVariable &value)
{
	QListWidgetItem *item = new QListWidgetItem(_list);
	auto container = new KeyValueListContainerWidget(this, _list->count());
	container->_key->setText(QString::fromStdString(key.UnresolvedValue()));
	container->_value->setText(
		QString::fromStdString(value.UnresolvedValue()));
	container->adjustSize();
	container->updateGeometry();
	_list->addItem(item);
	_list->setItemWidget(item, container);
	UpdateListSize();
}

KeyValueListContainerWidget::KeyValueListContainerWidget(QWidget *parent,
							 int index)
	: QWidget(parent),
	  _key(new QLabel("Key", this)),
	  _value(new QLabel("Value", this)),
	  _index(index)
{
	auto layout = new QHBoxLayout();
	layout->addWidget(_key);
	layout->addWidget(_value);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
}

} // namespace advss
