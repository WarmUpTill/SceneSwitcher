#include "list-editor.hpp"
#include "ui-helpers.hpp"

namespace advss {

ListEditor::ListEditor(QWidget *parent, bool reorder)
	: QWidget(parent),
	  _list(new QListWidget()),
	  _add(new QPushButton()),
	  _remove(new QPushButton()),
	  _up(new QPushButton()),
	  _down(new QPushButton()),
	  _controlsLayout(new QHBoxLayout()),
	  _mainLayout(new QVBoxLayout())
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

	_controlsLayout->setContentsMargins(0, 0, 0, 0);
	_controlsLayout->addWidget(_add);
	_controlsLayout->addWidget(_remove);
	if (reorder) {
		auto line = new QFrame();
		line->setFrameShape(QFrame::VLine);
		line->setFrameShadow(QFrame::Sunken);
		_controlsLayout->addWidget(line);
		_controlsLayout->addWidget(_up);
		_controlsLayout->addWidget(_down);
	}
	_controlsLayout->addStretch();

	_mainLayout->setContentsMargins(0, 0, 0, 0);
	_mainLayout->addWidget(_list);
	_mainLayout->addLayout(_controlsLayout);
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
