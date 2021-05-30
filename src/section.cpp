#include "headers/section.hpp"
#include "headers/utility.hpp"

#include <QPropertyAnimation>
#include <QEvent>

Section::Section(const int animationDuration, QWidget *parent)
	: QWidget(parent), _animationDuration(animationDuration)
{
	_toggleButton = new QToolButton(this);
	_headerLine = new QFrame(this);
	_mainLayout = new QGridLayout(this);
	_headerWidgetLayout = new QHBoxLayout();

	_toggleButton->setStyleSheet("QToolButton {border: none;}");
	_toggleButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
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

void Section::Collapse(bool collapse)
{
	_toggleButton->setChecked(collapse);
	_toggleButton->setArrowType(!collapse ? Qt::ArrowType::DownArrow
					      : Qt::ArrowType::RightArrow);
	_toggleAnimation->setDirection(!collapse
					       ? QAbstractAnimation::Forward
					       : QAbstractAnimation::Backward);
	_transitioning = true;
	_collapsed = collapse;
	_toggleAnimation->start();
}

void Section::SetContent(QWidget *w, bool collapsed)
{
	CleanUpPreviousContent();
	delete _contentArea;

	// Setup contentArea
	_contentArea = new QScrollArea(this);
	_contentArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	_contentArea->setStyleSheet("QScrollArea { border: none; }");
	_contentArea->setMaximumHeight(0);
	_contentArea->setMinimumHeight(0);

	w->installEventFilter(this);
	_content = w;
	auto newLayout = new QVBoxLayout();
	newLayout->setContentsMargins(0, 0, 0, 0);
	newLayout->addWidget(w);
	_contentArea->setLayout(newLayout);
	_mainLayout->addWidget(_contentArea, 1, 0, 1, 3);

	_headerHeight = sizeHint().height() - _contentArea->maximumHeight();
	_contentHeight = newLayout->sizeHint().height();

	if (collapsed) {
		this->setMinimumHeight(_headerHeight);
		_contentArea->setMaximumHeight(0);
	} else {
		this->setMinimumHeight(_headerHeight + _contentHeight);
		_contentArea->setMaximumHeight(_contentHeight);
	}
	SetupAnimations();
	Collapse(collapsed);
}

void Section::AddHeaderWidget(QWidget *w)
{
	_headerWidgetLayout->addWidget(w);
}

bool Section::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::Resize && !_transitioning && !_collapsed) {
		_contentHeight = _content->sizeHint().height();
		setMaximumHeight(_headerHeight + _contentHeight);
		setMinimumHeight(_headerHeight + _contentHeight);
		_contentArea->setMaximumHeight(_contentHeight);
		SetupAnimations();
	}
	return QObject::eventFilter(obj, event);
}

void Section::SetupAnimations()
{
	delete _toggleAnimation;

	_toggleAnimation = new QParallelAnimationGroup(this);
	_toggleAnimation->addAnimation(
		new QPropertyAnimation(this, "minimumHeight"));
	_toggleAnimation->addAnimation(
		new QPropertyAnimation(this, "maximumHeight"));
	_toggleAnimation->addAnimation(
		new QPropertyAnimation(_contentArea, "maximumHeight"));

	for (int i = 0; i < _toggleAnimation->animationCount() - 1; ++i) {
		QPropertyAnimation *SectionAnimation =
			static_cast<QPropertyAnimation *>(
				_toggleAnimation->animationAt(i));
		SectionAnimation->setDuration(_animationDuration);
		SectionAnimation->setStartValue(_headerHeight);
		SectionAnimation->setEndValue(_headerHeight + _contentHeight);
	}

	QPropertyAnimation *contentAnimation =
		static_cast<QPropertyAnimation *>(_toggleAnimation->animationAt(
			_toggleAnimation->animationCount() - 1));
	contentAnimation->setDuration(_animationDuration);
	contentAnimation->setStartValue(0);
	contentAnimation->setEndValue(_contentHeight);

	QWidget::connect(_toggleAnimation, SIGNAL(finished()), this,
			 SLOT(AnimationFinished()));
}

void Section::CleanUpPreviousContent()
{
	// Clean up previous content
	if (_contentArea) {
		auto oldLayout = _contentArea->layout();
		if (oldLayout) {
			clearLayout(oldLayout);
			delete oldLayout;
		}
	}
}

void Section::AnimationFinished()
{
	_transitioning = false;
}
