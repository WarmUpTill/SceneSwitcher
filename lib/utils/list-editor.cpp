#include "list-editor.hpp"
#include "ui-helpers.hpp"

#include <QEvent>

namespace advss {

ListEditor::ListEditor(QWidget *parent, bool reorder)
	: QWidget(parent),
	  _list(new QListWidget()),
	  _controls(new ListControls(this, reorder)),
	  _mainLayout(new QVBoxLayout()),
	  _placeholder(new QLabel(_list->viewport()))
{
	QWidget::connect(_controls, SIGNAL(Add()), this, SLOT(Add()));
	QWidget::connect(_controls, SIGNAL(Remove()), this, SLOT(Remove()));
	QWidget::connect(_controls, SIGNAL(Up()), this, SLOT(Up()));
	QWidget::connect(_controls, SIGNAL(Down()), this, SLOT(Down()));
	QWidget::connect(_list, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
			 this, SLOT(Clicked(QListWidgetItem *)));
	QWidget::connect(_list->model(),
			 SIGNAL(rowsInserted(QModelIndex, int, int)), this,
			 SLOT(UpdatePlaceholder()));
	QWidget::connect(_list->model(),
			 SIGNAL(rowsRemoved(QModelIndex, int, int)), this,
			 SLOT(UpdatePlaceholder()));
	QWidget::connect(_list->model(), SIGNAL(modelReset()), this,
			 SLOT(UpdatePlaceholder()));

	_placeholder->setAlignment(Qt::AlignCenter);
	_placeholder->setWordWrap(true);
	_placeholder->hide();
	_list->viewport()->installEventFilter(this);

	_mainLayout->setContentsMargins(0, 0, 0, 0);
	_mainLayout->addWidget(_list);
	_mainLayout->addWidget(_controls);
	setLayout(_mainLayout);
}

void ListEditor::SetPlaceholderText(const QString &text)
{
	_placeholder->setText(text);
	UpdatePlaceholder();
}

void ListEditor::SetMinListHeight(int value)
{
	_minHeight = value;
}

void ListEditor::SetMaxListHeight(int value)
{
	_maxHeight = value;
}

void ListEditor::UpdatePlaceholder()
{
	bool visible = !_placeholder->text().isEmpty() && _list->count() == 0;
	_placeholder->setVisible(visible);
	if (visible) {
		_placeholder->setGeometry(_list->viewport()->rect());
	}
}

bool ListEditor::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == _list->viewport() && event->type() == QEvent::Resize &&
	    _placeholder->isVisible()) {
		_placeholder->setGeometry(_list->viewport()->rect());
	}
	return QWidget::eventFilter(obj, event);
}

void ListEditor::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	// This is necessary as the list viewport might not be updated yet while
	// the list was hidden.
	// Thus, previous calls to UpdateListSize() might not have resized the
	// widget correctly, for example due to not regarding the horizontal
	// scrollbar yet.
	UpdateListSize();
}

static QListWidgetItem *getItemFromWidget(QListWidget *list, QWidget *widget)
{
	for (int i = 0; i < list->count(); i++) {
		auto item = list->item(i);
		if (!item) {
			continue;
		}
		auto itemWidget = list->itemWidget(item);
		if (itemWidget == widget) {
			return item;
		}
	}
	return nullptr;
}

int ListEditor::GetIndexOfSignal() const
{
	auto sender = this->sender();
	if (!sender) {
		return -1;
	}
	auto widget = qobject_cast<QWidget *>(sender);
	if (!widget) {
		return -1;
	}
	return _list->row(getItemFromWidget(_list, widget));
}

void ListEditor::UpdateListSize()
{
	SetHeightToContentHeight(_list);

	if (_list->count() == 0 && !_placeholder->text().isEmpty()) {
		auto height = _list->fontMetrics().height() * 3;
		_list->setMinimumHeight(height);
		_list->setMaximumHeight(height);
	}

	if (_minHeight >= 0 && _list->minimumHeight() != _minHeight) {
		_list->setMinimumHeight(_minHeight);
	}

	if (_maxHeight >= 0 && _list->maximumHeight() != _maxHeight) {
		if (_list->minimumHeight() > _maxHeight) {
			_list->setMinimumHeight(_maxHeight);
		}
		_list->setMaximumHeight(_maxHeight);
	}

	adjustSize();
	updateGeometry();
}

} // namespace advss
