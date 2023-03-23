#include "macro-dock.hpp"
#include "macro.hpp"
#include "utility.hpp"

#include <QLayout>
#include <obs-module.h>

MacroDock::MacroDock(Macro *m, QWidget *parent)
	: OBSDock(parent),
	  _run(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.macroDock.run"))),
	  _pauseToggle(new QPushButton()),
	  _macro(m)
{
	if (_macro) {
		setWindowTitle(QString::fromStdString(_macro->Name()));
	} else {
		setWindowTitle("<deleted macro>");
	}

	setFeatures(DockWidgetClosable | DockWidgetMovable |
		    DockWidgetFloatable);

	QWidget::connect(_run, SIGNAL(clicked()), this, SLOT(RunClicked()));
	QWidget::connect(_pauseToggle, SIGNAL(clicked()), this,
			 SLOT(PauseToggleClicked()));

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_run);
	layout->addWidget(_pauseToggle);

	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdatePauseText()));
	_timer.start(1000);

	UpdatePauseText();

	// QFrame wrapper is necessary to avoid dock being partially
	// transparent
	QFrame *wrapper = new QFrame;
	wrapper->setFrameShape(QFrame::StyledPanel);
	wrapper->setFrameShadow(QFrame::Sunken);
	wrapper->setLayout(layout);
	setWidget(wrapper);
}

void MacroDock::SetName(const QString &name)
{
	setWindowTitle(name);
}

void MacroDock::RunClicked()
{
	if (!_macro) {
		return;
	}

	auto ret = _macro->PerformActions();
	if (!ret) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroTab.runFail");
		DisplayMessage(err.arg(QString::fromStdString(_macro->Name())));
	}
}

void MacroDock::PauseToggleClicked()
{
	if (!_macro) {
		return;
	}

	_macro->SetPaused(!_macro->Paused());
	UpdatePauseText();
}

void MacroDock::UpdatePauseText()
{
	if (!_macro) {
		return;
	}

	_pauseToggle->setText(
		_macro->Paused()
			? obs_module_text("AdvSceneSwitcher.macroDock.unpause")
			: obs_module_text("AdvSceneSwitcher.macroDock.pause"));
}
