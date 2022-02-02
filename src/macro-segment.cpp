#include "headers/macro-segment.hpp"
#include "headers/macro-controls.hpp"
#include "headers/section.hpp"

#include <obs.hpp>
#include <QEvent>
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

MacroSegmentEdit::MacroSegmentEdit(bool verticalControls, QWidget *parent)
	: QWidget(parent)
{
	_section = new Section(300);
	_headerInfo = new QLabel();
	_controls = new MacroEntryControls(verticalControls);

	_enterTimer.setSingleShot(true);
	_enterTimer.setInterval(1000);
	_leaveTimer.setSingleShot(true);
	_leaveTimer.setInterval(1000);

	_frame = new QFrame;
	_frame->setObjectName("segmentFrame");
	_highLightFrameLayout = new QVBoxLayout;
	_frame->setStyleSheet(
		"#segmentFrame { border-radius: 4px; background-color: rgba(0,0,0,50); }");
	_frame->setLayout(_highLightFrameLayout);
	// Set background transparent to avoid blocking highlight frame
	setStyleSheet("QCheckBox { background-color: rgba(0,0,0,0); }\
		       QLabel { background-color: rgba(0,0,0,0); }");

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

	// Control signals
	QWidget::connect(_controls, &MacroEntryControls::Add, this,
			 &MacroSegmentEdit::Add);
	QWidget::connect(_controls, &MacroEntryControls::Remove, this,
			 &MacroSegmentEdit::Remove);
	QWidget::connect(_controls, &MacroEntryControls::Up, this,
			 &MacroSegmentEdit::Up);
	QWidget::connect(_controls, &MacroEntryControls::Down, this,
			 &MacroSegmentEdit::Down);
	QWidget::connect(&_enterTimer, &QTimer::timeout, this,
			 &MacroSegmentEdit::ShowControls);
	QWidget::connect(&_leaveTimer, &QTimer::timeout, this,
			 &MacroSegmentEdit::HideControls);
}

void MacroSegmentEdit::HeaderInfoChanged(const QString &text)
{
	_headerInfo->setVisible(!text.isEmpty());
	_headerInfo->setText(text);
}

void MacroSegmentEdit::Add()
{
	if (Data()) {
		// Insert after current entry
		emit AddAt(Data()->GetIndex() + 1);
	}
}

void MacroSegmentEdit::Remove()
{
	if (Data()) {
		emit RemoveAt(Data()->GetIndex());
	}
}

void MacroSegmentEdit::Up()
{
	if (Data()) {
		emit UpAt(Data()->GetIndex());
	}
}

void MacroSegmentEdit::Down()
{
	if (Data()) {
		emit DownAt(Data()->GetIndex());
	}
}

void MacroSegmentEdit::Collapsed(bool collapsed)
{
	if (Data()) {
		Data()->SetCollapsed(collapsed);
	}
}

void MacroSegmentEdit::ShowControls()
{
	_controls->Show(true);
}

void MacroSegmentEdit::HideControls()
{
	_controls->Show(false);
}

void MacroSegmentEdit::enterEvent(QEvent *)
{
	_enterTimer.start();
	_leaveTimer.stop();
}

void MacroSegmentEdit::leaveEvent(QEvent *)
{
	_enterTimer.stop();
	_leaveTimer.start();
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
