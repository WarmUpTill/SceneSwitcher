#include <QPropertyAnimation>
#include "headers/section.hpp"
#include "headers/utility.hpp"

Section::Section(const int animationDuration, QWidget *parent)
	: QWidget(parent), _animationDuration(animationDuration)
{
	_toggleButton = new QToolButton(this);
	_headerLine = new QFrame(this);
	_mainLayout = new QGridLayout(this);
	_headerWidgetLayout = new QHBoxLayout();

	_toggleButton->setStyleSheet("QToolButton {border: none;}");
	_toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	_toggleButton->setArrowType(Qt::ArrowType::RightArrow);
	_toggleButton->setCheckable(true);
	_toggleButton->setChecked(true);

	_headerLine->setFrameShape(QFrame::HLine);
	_headerLine->setFrameShadow(QFrame::Sunken);
	_headerLine->setSizePolicy(QSizePolicy::Expanding,
				   QSizePolicy::Maximum);

	// Don't waste space
	_mainLayout->setVerticalSpacing(0);
	_mainLayout->setContentsMargins(0, 0, 0, 0);

	// But add some spacing for widgets in header
	_headerWidgetLayout->setSpacing(11);
	_headerWidgetLayout->addWidget(_toggleButton);

	int row = 0;
	_mainLayout->addLayout(_headerWidgetLayout, row, 0, 1, 1,
			       Qt::AlignLeft);
	_mainLayout->addWidget(_headerLine, row++, 2, 1, 1);
	setLayout(_mainLayout);

	connect(_toggleButton, &QToolButton::toggled, this, &Section::Collapse);
}

void Section::Collapse(bool collapsed)
{
	_toggleButton->setArrowType(!collapsed ? Qt::ArrowType::DownArrow
					       : Qt::ArrowType::RightArrow);
	_toggleAnimation->setDirection(!collapsed
					       ? QAbstractAnimation::Forward
					       : QAbstractAnimation::Backward);
	_toggleAnimation->start();
}

void Section::SetContent(QWidget *w)
{
	if (_contentArea) {
		auto oldLayout = _contentArea->layout();
		if (oldLayout) {
			clearLayout(oldLayout);
			delete oldLayout;
		}
	}

	delete _contentArea;
	_contentArea = new QScrollArea(this);
	_contentArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	// Start out collapsed
	_contentArea->setMaximumHeight(0);
	_contentArea->setMinimumHeight(0);

	auto newLayout = new QVBoxLayout();
	newLayout->addWidget(w);
	w->show();
	_contentArea->setLayout(newLayout);

	delete _toggleAnimation;
	_toggleAnimation = new QParallelAnimationGroup(this);
	_toggleAnimation->addAnimation(
		new QPropertyAnimation(this, "minimumHeight"));
	_toggleAnimation->addAnimation(
		new QPropertyAnimation(this, "maximumHeight"));
	_toggleAnimation->addAnimation(
		new QPropertyAnimation(_contentArea, "maximumHeight"));

	connect(_toggleButton, &QToolButton::toggled, this, &Section::Collapse);
	_mainLayout->addWidget(_contentArea, 1, 0, 1, 3);

	// Animation Setup
	const auto collapsedHeight =
		sizeHint().height() - _contentArea->maximumHeight();
	auto contentHeight = newLayout->sizeHint().height();

	for (int i = 0; i < _toggleAnimation->animationCount() - 1; ++i) {
		QPropertyAnimation *SectionAnimation =
			static_cast<QPropertyAnimation *>(
				_toggleAnimation->animationAt(i));
		SectionAnimation->setDuration(_animationDuration);
		SectionAnimation->setStartValue(collapsedHeight);
		SectionAnimation->setEndValue(collapsedHeight + contentHeight);
	}

	QPropertyAnimation *contentAnimation =
		static_cast<QPropertyAnimation *>(_toggleAnimation->animationAt(
			_toggleAnimation->animationCount() - 1));
	contentAnimation->setDuration(_animationDuration);
	contentAnimation->setStartValue(0);
	contentAnimation->setEndValue(contentHeight);
}

void Section::AddHeaderWidget(QWidget *w)
{
	_headerWidgetLayout->addWidget(w);
}
