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

MacroSegmentEdit::MacroSegmentEdit(QWidget *parent) : QWidget(parent)
{
	_section = new Section(300);
	_headerInfo = new QLabel();
	_controls = new MacroEntryControls();

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

void MacroSegmentEdit::enterEvent(QEvent *)
{
	_controls->Show(true);
}

void MacroSegmentEdit::leaveEvent(QEvent *)
{
	_controls->Show(false);
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
