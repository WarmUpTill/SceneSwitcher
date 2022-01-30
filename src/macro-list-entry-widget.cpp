#include "headers/macro-list-entry-widget.hpp"
#include "headers/macro.hpp"

MacroListEntryWidget::MacroListEntryWidget(std::shared_ptr<Macro> macro,
					   QWidget *parent)
	: QWidget(parent), _macro(macro)
{
	_name = new QLabel(QString::fromStdString(macro->Name()));
	_running = new QCheckBox();
	_running->setChecked(!macro->Paused());
	connect(_running, SIGNAL(stateChanged(int)), this,
		SLOT(PauseChanged(int)));

	setStyleSheet("\
		QCheckBox { background-color: rgba(0,0,0,0); }\
		QLabel  { background-color: rgba(0,0,0,0); }");

	auto layout = new QHBoxLayout;
	layout->setContentsMargins(3, 3, 3, 3);
	layout->addWidget(_running);
	layout->addWidget(_name);
	layout->addStretch();
	setLayout(layout);
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
