#include "string-list.hpp"
#include "name-dialog.hpp"
#include "utility.hpp"

#include <QLayout>

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

StringListEdit::StringListEdit(QWidget *parent, const QString &addString,
			       const QString &addStringDescription)
	: QWidget(parent),
	  _list(new QListWidget()),
	  _add(new QPushButton()),
	  _remove(new QPushButton()),
	  _up(new QPushButton()),
	  _down(new QPushButton()),
	  _addString(addString),
	  _addStringDescription(addStringDescription)
{
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
			 this, SLOT(Clicked(QListWidgetItem *)));

	auto controlLayout = new QHBoxLayout;
	controlLayout->setContentsMargins(0, 0, 0, 0);
	controlLayout->addWidget(_add);
	controlLayout->addWidget(_remove);
	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Sunken);
	controlLayout->addWidget(line);
	controlLayout->addWidget(_up);
	controlLayout->addWidget(_down);
	controlLayout->addStretch();

	auto mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addWidget(_list);
	mainLayout->addLayout(controlLayout);
	setLayout(mainLayout);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
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
	SetListSize();
}

void StringListEdit::Add()
{
	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(
		this, _addString, _addStringDescription, name, "", 170, false);

	if (!accepted || name.empty()) {
		return;
	}
	StringVariable string = name;
	QVariant v = QVariant::fromValue(string);
	QListWidgetItem *item = new QListWidgetItem(
		QString::fromStdString(string.UnresolvedValue()), _list);
	item->setData(Qt::UserRole, string);

	_stringList << string;
	SetListSize();

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
	SetListSize();

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
	std::string name;
	bool accepted = AdvSSNameDialog::AskForName(this, _addString,
						    _addStringDescription, name,
						    item->text(), 170, false);

	if (!accepted || name.empty()) {
		return;
	}

	StringVariable string = name;
	QVariant v = QVariant::fromValue(string);
	item->setText(QString::fromStdString(string.UnresolvedValue()));
	item->setData(Qt::UserRole, string);
	int idx = _list->currentRow();
	_stringList[idx] = string;

	StringListChanged(_stringList);
}

void StringListEdit::SetListSize()
{
	setHeightToContentHeight(_list);
	adjustSize();
	updateGeometry();
}
