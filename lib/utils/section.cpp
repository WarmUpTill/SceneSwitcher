#include "section.hpp"
#include "layout-helpers.hpp"

#include <QPropertyAnimation>
#include <QEvent>

namespace advss {

Section::Section(const int animationDuration, QWidget *parent)
	: QWidget(parent),
	  _animationDuration(animationDuration)
{
	_toggleButton = new QToolButton(this);
	_headerLine = new QFrame(this);
	_mainLayout = new QGridLayout(this);
	_headerWidgetLayout = new QHBoxLayout();

	_toggleButton->setStyleSheet(
		"QToolButton {border: none; background-color: rgba(0,0,0,0);}");
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

	_mainLayout->addLayout(_headerWidgetLayout, 0, 0, 1, 1, Qt::AlignLeft);
	_mainLayout->addWidget(_headerLine, 0, 2, 1, 1);
	setLayout(_mainLayout);

	connect(_toggleButton, &QToolButton::toggled, this, &Section::Collapse);
}

void Section::Collapse(bool collapse)
{
	if (_contentHeight != _content->sizeHint().height()) {
		_contentHeight = _content->sizeHint().height();
	}
	SetupAnimations();

	_toggleButton->setChecked(collapse);
	_toggleButton->setArrowType(collapse ? Qt::ArrowType::RightArrow
					     : Qt::ArrowType::DownArrow);
	if (!_toggleAnimation) {
		_collapsed = collapse;
		emit Collapsed(collapse);
		return;
	}

	_toggleAnimation->setDirection(collapse ? QAbstractAnimation::Backward
						: QAbstractAnimation::Forward);
	_transitioning = true;
	_collapsed = collapse;
	_toggleAnimation->start();
	emit Collapsed(collapse);
}

void Section::SetContent(QWidget *w)
{
	SetContent(w, _collapsed);
}

void Section::SetContent(QWidget *w, bool collapsed)
{
	CleanUpPreviousContent();
	delete _contentArea;

	// Setup contentArea
	_contentArea = new QWidget(this);
	_contentArea->setObjectName("macroSegmentContent");
	_contentArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	_contentArea->setStyleSheet(
		"#macroSegmentContent { border: none; background-color: rgba(0,0,0,0); }");
	_contentArea->setMaximumHeight(0);
	_contentArea->setMinimumHeight(0);

	_content = w;
	_content->installEventFilter(this);
	auto newLayout = new QVBoxLayout();
	newLayout->setContentsMargins(0, 0, 0, 0);
	newLayout->addWidget(w);
	_contentArea->setLayout(newLayout);
	_mainLayout->addWidget(_contentArea, 1, 0, 1, 3);

	_headerHeight = sizeHint().height() - _contentArea->maximumHeight();
	_contentHeight = _content->sizeHint().height();

	SetupAnimations();

	setMinimumHeight(_headerHeight);
	if (collapsed) {
		_contentArea->setMaximumHeight(0);
	} else {
		_contentArea->setMaximumHeight(QWIDGETSIZE_MAX);
	}
	const QSignalBlocker b(_toggleButton);
	_toggleButton->setChecked(collapsed);
	_toggleButton->setArrowType(collapsed ? Qt::ArrowType::RightArrow
					      : Qt::ArrowType::DownArrow);
	_collapsed = collapsed;
}

void Section::AddHeaderWidget(QWidget *w)
{
	_headerWidgetLayout->addWidget(w);
}

void Section::SetCollapsed(bool collapsed)
{
	if (_collapsed == collapsed) {
		return;
	}
	Collapse(collapsed);
}

void Section::SetupAnimations()
{
	if (_toggleAnimation) {
		_toggleAnimation->deleteLater();
	}

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

	auto contentAnimation =
		static_cast<QPropertyAnimation *>(_toggleAnimation->animationAt(
			_toggleAnimation->animationCount() - 1));
	contentAnimation->setDuration(_animationDuration);
	contentAnimation->setStartValue(0);
	contentAnimation->setEndValue(_contentHeight);

	QWidget::connect(_toggleAnimation, SIGNAL(finished()), this,
			 SLOT(AnimationFinish()));
}

void Section::CleanUpPreviousContent()
{
	if (_contentArea) {
		auto oldLayout = _contentArea->layout();
		if (oldLayout) {
			ClearLayout(oldLayout);
			delete oldLayout;
		}
	}
}

void Section::AnimationFinish()
{
	_transitioning = false;

	// Allow resizing if we are not currently transitioning or collapsed
	if (!_collapsed) {
		setMinimumHeight(0);
		setMaximumHeight(QWIDGETSIZE_MAX);
		_contentArea->setMaximumHeight(QWIDGETSIZE_MAX);
	}

	emit AnimationFinished();
}

} // namespace advss
