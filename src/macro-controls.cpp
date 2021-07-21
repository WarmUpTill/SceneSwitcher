#include "headers/macro-controls.hpp"

MacroEntryControls::MacroEntryControls(const int animationDuration,
				       QWidget *parent)
	: QWidget(parent)
{
	_add = new QPushButton(this);
	_add->setMaximumSize(QSize(22, 22));
	_add->setProperty("themeID",
			  QVariant(QString::fromUtf8("addIconSmall")));
	_add->setFlat(true);
	_remove = new QPushButton(this);
	_remove->setMaximumSize(QSize(22, 22));
	_remove->setProperty("themeID",
			     QVariant(QString::fromUtf8("removeIconSmall")));
	_remove->setFlat(true);
	_up = new QPushButton(this);
	_up->setMaximumSize(QSize(22, 22));
	_up->setProperty("themeID",
			 QVariant(QString::fromUtf8("upArrowIconSmall")));
	_up->setFlat(true);
	_down = new QPushButton(this);
	_down->setMaximumSize(QSize(22, 22));
	_down->setProperty("themeID",
			   QVariant(QString::fromUtf8("downArrowIconSmall")));
	_down->setFlat(true);

	auto line = new QFrame(this);
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Sunken);
	line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

	auto mainLayout = new QHBoxLayout(this);

	mainLayout->addWidget(_add);
	mainLayout->addWidget(_remove);

	auto *separator = new QFrame(this);
	separator->setFrameShape(QFrame::VLine);
	separator->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(separator);

	mainLayout->addWidget(_up);
	mainLayout->addWidget(_down);
	mainLayout->addWidget(line);

	mainLayout->setContentsMargins(0, 0, 0, 0);

	_animation = new QPropertyAnimation(this, "maximumHeight");

	_animation->setDuration(animationDuration);
	_animation->setStartValue(0);
	_animation->setEndValue(mainLayout->sizeHint().height());

	setMaximumHeight(0);
	setLayout(mainLayout);

	connect(_add, &QPushButton::clicked, this, &MacroEntryControls::Add);
	connect(_remove, &QPushButton::clicked, this,
		&MacroEntryControls::Remove);
	connect(_up, &QPushButton::clicked, this, &MacroEntryControls::Up);
	connect(_down, &QPushButton::clicked, this, &MacroEntryControls::Down);
}

void MacroEntryControls::Show(bool visible)
{
	_animation->setDirection(visible ? QAbstractAnimation::Forward
					 : QAbstractAnimation::Backward);
	_animation->start();
}
