#include "headers/macro-segment.hpp"
#include "headers/section.hpp"
#include "headers/utility.hpp"

#include <obs.hpp>
#include <QEvent>
#include <QMouseEvent>
#include <QLabel>
#include <QScrollBar>

bool MacroSegment::Save(obs_data_t *obj)
{
	obs_data_set_bool(obj, "collapsed", static_cast<int>(_collapsed));
	return true;
}

bool MacroSegment::Load(obs_data_t *obj)
{
	_collapsed = obs_data_get_bool(obj, "collapsed");
	return true;
}

std::string MacroSegment::GetShortDesc()
{
	return "";
}

void MacroSegment::SetHighlight()
{
	_highlight = true;
}

bool MacroSegment::Highlight()
{
	if (_highlight) {
		_highlight = false;
		return true;
	}
	return false;
}

MouseWheelWidgetAdjustmentGuard::MouseWheelWidgetAdjustmentGuard(QObject *parent)
	: QObject(parent)
{
}

bool MouseWheelWidgetAdjustmentGuard::eventFilter(QObject *o, QEvent *e)
{
	const QWidget *widget = static_cast<QWidget *>(o);
	if (e->type() == QEvent::Wheel && widget && !widget->hasFocus()) {
		e->ignore();
		return true;
	}

	return QObject::eventFilter(o, e);
}

MacroSegmentEdit::MacroSegmentEdit(bool highlight, QWidget *parent)
	: QWidget(parent), _showHighlight(highlight)
{
	_section = new Section(300);
	_headerInfo = new QLabel();

	_frame = new QFrame;
	_frame->setObjectName("segmentFrame");
	_selectionFrameLayout = new QVBoxLayout;
	SetSelected(false);
	_frame->setLayout(_selectionFrameLayout);
	// Set background transparent to avoid blocking highlight frame
	setStyleSheet("QCheckBox { background-color: rgba(0,0,0,0); }"
		      "QLabel { background-color: rgba(0,0,0,0); }"
		      "QSlider { background-color: rgba(0,0,0,0); }");

	// Keep the size of macro segments consistent, even if there is room in
	// the edit areas
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QWidget::connect(_section, &Section::Collapsed, this,
			 &MacroSegmentEdit::Collapsed);

	// Macro signals
	QWidget::connect(parent, SIGNAL(MacroAdded(const QString &)), this,
			 SIGNAL(MacroAdded(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SIGNAL(MacroRemoved(const QString &)));
	QWidget::connect(parent,
			 SIGNAL(MacroRenamed(const QString &, const QString)),
			 this,
			 SIGNAL(MacroRenamed(const QString &, const QString)));

	// Scene group signals
	QWidget::connect(parent, SIGNAL(SceneGroupAdded(const QString &)), this,
			 SIGNAL(SceneGroupAdded(const QString &)));
	QWidget::connect(parent, SIGNAL(SceneGroupRemoved(const QString &)),
			 this, SIGNAL(SceneGroupRemoved(const QString &)));
	QWidget::connect(
		parent,
		SIGNAL(SceneGroupRenamed(const QString &, const QString)), this,
		SIGNAL(SceneGroupRenamed(const QString &, const QString)));

	_timer.setInterval(1500);
	connect(&_timer, SIGNAL(timeout()), this, SLOT(Highlight()));
	_timer.start();
}

void MacroSegmentEdit::HeaderInfoChanged(const QString &text)
{
	_headerInfo->setVisible(!text.isEmpty());
	_headerInfo->setText(text);
}

void MacroSegmentEdit::Collapsed(bool collapsed)
{
	if (Data()) {
		Data()->SetCollapsed(collapsed);
	}
}

void MacroSegmentEdit::Highlight()
{
	if (!Data()) {
		return;
	}

	if (_showHighlight && Data()->Highlight()) {
		PulseWidget(this, Qt::green, QColor(0, 0, 0, 0), true);
	}
}

void MacroSegmentEdit::EnableHighlight(bool value)
{
	_showHighlight = value;
}

void MacroSegmentEdit::SetFocusPolicyOfWidgets()
{
	QList<QWidget *> widgets = this->findChildren<QWidget *>();
	for (auto w : widgets) {
		w->setFocusPolicy(Qt::StrongFocus);
		// Ignore QScrollBar as there is no danger of accidentally modifying anything
		// and long expanded QComboBox would be difficult to interact with otherwise.
		if (qobject_cast<QScrollBar *>(w)) {
			continue;
		}
		w->installEventFilter(new MouseWheelWidgetAdjustmentGuard(w));
	}
}

void MacroSegmentEdit::SetCollapsed(bool collapsed)
{
	_section->SetCollapsed(collapsed);
}

void MacroSegmentEdit::SetSelected(bool selected)
{
	if (selected) {
		_frame->setStyleSheet("#segmentFrame {"
				      "border-color: rgba(0, 0, 0, 255);"
				      "border-width: 2px;"
				      "border-style: dashed;"
				      "border-radius: 4px;"
				      "background-color: rgba(0,0,0,100);"
				      "}");
	} else {
		_frame->setStyleSheet("#segmentFrame {"
				      "border-color: rgba(0, 0, 0, 0);"
				      "border-width: 2px;"
				      "border-style: dashed;"
				      "border-radius: 4px;"
				      "background-color: rgba(0,0,0,50);"
				      "}");
	}
}
