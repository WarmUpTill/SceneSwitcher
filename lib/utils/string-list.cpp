#include "string-list.hpp"
#include "name-dialog.hpp"
#include "ui-helpers.hpp"

#include <QLayout>
#include <QTimer>

namespace advss {

bool StringList::Save(obs_data_t *obj, const char *name,
		      const char *elementName) const
{
	obs_data_array_t *strings = obs_data_array_create();
	for (auto &string : *this) {
		obs_data_t *array_obj = obs_data_create();
		string.Save(array_obj, elementName);
		obs_data_array_push_back(strings, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, name, strings);
	obs_data_array_release(strings);
	return true;
}

bool StringList::Load(obs_data_t *obj, const char *name,
		      const char *elementName)
{
	clear();
	obs_data_array_t *strings = obs_data_get_array(obj, name);
	size_t count = obs_data_array_count(strings);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(strings, i);
		StringVariable string;
		string.Load(array_obj, elementName);
		*this << string;
		obs_data_release(array_obj);
	}
	obs_data_array_release(strings);
	return true;
}

void StringList::ResolveVariables()
{
	for (auto &value : *this) {
		value.ResolveVariables();
	}
}

StringListEdit::StringListEdit(
	QWidget *parent, const QString &addString,
	const QString &addStringDescription, int maxStringSize,
	const std::function<bool(const std::string &)> &filter,
	const std::function<void(std::string &input)> &preprocess)
	: ListEditor(parent),
	  _addString(addString),
	  _addStringDescription(addStringDescription),
	  _maxStringSize(maxStringSize),
	  _filterCallback(filter),
	  _preprocessCallback(preprocess)
{
	if (_addString.isEmpty()) {
		_addString = obs_module_text("AdvSceneSwitcher.windowTitle");
	}
}

void StringListEdit::SetStringList(const StringList &list)
{
	_stringList = list;
	_list->clear();
	for (const auto &string : list) {
		QListWidgetItem *item = new QListWidgetItem(
			QString::fromStdString(string.UnresolvedValue()),
			_list);
		item->setData(Qt::UserRole, string);
	}
	UpdateListSize();
}

void StringListEdit::SetMaxStringSize(int size)
{
	_maxStringSize = size;
}

void StringListEdit::Add()
{
	std::string entry;
	bool accepted = NameDialog::AskForName(this, _addString,
					       _addStringDescription, entry, "",
					       _maxStringSize, false);
	if (!accepted) {
		return;
	}

	_preprocessCallback(entry);
	if (_filterCallback(entry)) {
		return;
	}
	StringVariable string = entry;
	QVariant v = QVariant::fromValue(string);
	QListWidgetItem *item = new QListWidgetItem(
		QString::fromStdString(string.UnresolvedValue()), _list);
	item->setData(Qt::UserRole, string);

	_stringList << string;

	// Delay resizing to make sure the list viewport was already updated
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });

	StringListChanged(_stringList);
}

void StringListEdit::Remove()
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

	// Delay resizing to make sure the list viewport was already updated
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });

	StringListChanged(_stringList);
}

void StringListEdit::Up()
{
	int idx = _list->currentRow();
	if (idx != -1 && idx != 0) {
		_list->insertItem(idx - 1, _list->takeItem(idx));
		_list->setCurrentRow(idx - 1);

		_stringList.move(idx, idx - 1);
	}
	StringListChanged(_stringList);
}

void StringListEdit::Down()
{
	int idx = _list->currentRow();
	if (idx != -1 && idx != _list->count() - 1) {
		_list->insertItem(idx + 1, _list->takeItem(idx));
		_list->setCurrentRow(idx + 1);

		_stringList.move(idx, idx + 1);
	}
	StringListChanged(_stringList);
}

void StringListEdit::Clicked(QListWidgetItem *item)
{
	std::string entry;
	bool accepted = NameDialog::AskForName(this, _addString,
					       _addStringDescription, entry,
					       item->text(), _maxStringSize,
					       false);
	if (!accepted) {
		return;
	}

	_preprocessCallback(entry);
	if (_filterCallback(entry)) {
		return;
	}

	StringVariable string = entry;
	QVariant v = QVariant::fromValue(string);
	item->setText(QString::fromStdString(string.UnresolvedValue()));
	item->setData(Qt::UserRole, string);
	int idx = _list->currentRow();
	_stringList[idx] = string;

	// Delay resizing to make sure the list viewport was already updated
	QTimer::singleShot(0, this, [this]() { UpdateListSize(); });

	StringListChanged(_stringList);
}

} // namespace advss
