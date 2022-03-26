#include "headers/macro-list-entry-widget.hpp"
#include "headers/macro.hpp"
#include "headers/utility.hpp"

MacroListEntryWidget::MacroListEntryWidget(std::shared_ptr<Macro> macro,
					   bool highlight, QWidget *parent)
	: QWidget(parent),
	  _name(new QLabel(QString::fromStdString(macro->Name()))),
	  _running(new QCheckBox),
	  _macro(macro),
	  _highlightExecutedMacros(highlight)
{
	_running->setChecked(!macro->Paused());

	setStyleSheet("\
		QCheckBox { background-color: rgba(0,0,0,0); }\
		QLabel  { background-color: rgba(0,0,0,0); }");

	auto layout = new QHBoxLayout;
	layout->setContentsMargins(3, 3, 3, 3);
	layout->addWidget(_running);
	layout->addWidget(_name);
	layout->addStretch();
	setLayout(layout);

	connect(_running, SIGNAL(stateChanged(int)), this,
		SLOT(PauseChanged(int)));
	connect(window(), SIGNAL(HighlightMacrosChanged(bool)), this,
		SLOT(EnableHighlight(bool)));
	_timer.setInterval(1500);
	connect(&_timer, SIGNAL(timeout()), this, SLOT(HighlightExecuted()));
	connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdatePaused()));
	_timer.start();
}

void MacroListEntryWidget::PauseChanged(int state)
{
	_macro->SetPaused(!state);
}

void MacroListEntryWidget::SetName(const QString &name)
{
	_name->setText(name);
}

void MacroListEntryWidget::SetMacro(std::shared_ptr<Macro> &m)
{
	_macro = m;
}

void MacroListEntryWidget::EnableHighlight(bool value)
{
	_highlightExecutedMacros = value;
}

void MacroListEntryWidget::HighlightExecuted()
{
	if (!_highlightExecutedMacros) {
		return;
	}

	if (_macro && _macro->WasExecutedRecently()) {
		PulseWidget(this, Qt::green, QColor(0, 0, 0, 0), true);
	}
}

void MacroListEntryWidget::UpdatePaused()
{
	const QSignalBlocker b(_running);
	_running->setChecked(!_macro->Paused());
}
