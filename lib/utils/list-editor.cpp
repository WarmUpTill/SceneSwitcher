#include "list-editor.hpp"
#include "ui-helpers.hpp"

namespace advss {

ListEditor::ListEditor(QWidget *parent, bool reorder)
	: QWidget(parent),
	  _list(new QListWidget()),
	  _controls(new ListControls(this, reorder)),
	  _mainLayout(new QVBoxLayout())
{
	QWidget::connect(_controls, SIGNAL(Add()), this, SLOT(Add()));
	QWidget::connect(_controls, SIGNAL(Remove()), this, SLOT(Remove()));
	QWidget::connect(_controls, SIGNAL(Up()), this, SLOT(Up()));
	QWidget::connect(_controls, SIGNAL(Down()), this, SLOT(Down()));
	QWidget::connect(_list, SIGNAL(itemDoubleClicked(QListWidgetItem *)),
			 this, SLOT(Clicked(QListWidgetItem *)));

	_mainLayout->setContentsMargins(0, 0, 0, 0);
	_mainLayout->addWidget(_list);
	_mainLayout->addWidget(_controls);
	setLayout(_mainLayout);
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

void ListEditor::UpdateListSize()
{
	SetHeightToContentHeight(_list);
	adjustSize();
	updateGeometry();
}

} // namespace advss
