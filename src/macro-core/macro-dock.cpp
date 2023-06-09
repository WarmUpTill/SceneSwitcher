#include "macro-dock.hpp"
#include "macro.hpp"
#include "utility.hpp"

#include <QLayout>

namespace advss {

MacroDock::MacroDock(Macro *m, QWidget *parent,
		     const StringVariable &runButtonText,
		     const StringVariable &pauseButtonText,
		     const StringVariable &unpauseButtonText,
		     const StringVariable &conditionsTrueText,
		     const StringVariable &conditionsFalseText)
	: OBSDock(parent),
	  _runButtonText(runButtonText),
	  _pauseButtonText(pauseButtonText),
	  _unpauseButtonText(unpauseButtonText),
	  _conditionsTrueText(conditionsTrueText),
	  _conditionsFalseText(conditionsFalseText),
	  _run(new QPushButton(runButtonText.c_str())),
	  _pauseToggle(new QPushButton()),
	  _statusText(new QLabel(conditionsFalseText.c_str())),
	  _macro(m)
{
	if (_macro) {
		setWindowTitle(QString::fromStdString(_macro->Name()));
		_run->setVisible(_macro->DockHasRunButton());
		_pauseToggle->setVisible(_macro->DockHasPauseButton());
		_statusText->setVisible(_macro->DockHasStatusLabel());
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
	layout->addWidget(_statusText);

	UpdateText();
	QWidget::connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdateText()));
	_timer.start(500);

	// QFrame wrapper is necessary to avoid dock being partially
	// transparent
	QFrame *wrapper = new QFrame;
	wrapper->setFrameShape(QFrame::StyledPanel);
	wrapper->setFrameShadow(QFrame::Sunken);
	wrapper->setLayout(layout);
	setWidget(wrapper);

	setFloating(true);
	hide();
}

void MacroDock::SetName(const QString &name)
{
	setWindowTitle(name);
}

void MacroDock::ShowRunButton(bool value)
{
	_run->setVisible(value);
}

void MacroDock::SetRunButtonText(const StringVariable &text)
{
	_runButtonText = text;
	_run->setText(text.c_str());
}

void MacroDock::ShowPauseButton(bool value)
{
	_pauseToggle->setVisible(value);
}

void MacroDock::SetPauseButtonText(const StringVariable &text)
{
	_pauseButtonText = text;
	UpdateText();
}

void MacroDock::SetUnpauseButtonText(const StringVariable &text)
{
	_unpauseButtonText = text;
	UpdateText();
}

void MacroDock::ShowStatusLabel(bool value)
{
	_statusText->setVisible(value);
}

void MacroDock::SetConditionsTrueText(const StringVariable &text)
{
	_conditionsTrueText = text;
	UpdateText();
}

void MacroDock::SetConditionsFalseText(const StringVariable &text)
{
	_conditionsFalseText = text;
	UpdateText();
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
	UpdateText();
}

void MacroDock::UpdateText()
{
	_run->setText(_runButtonText.c_str());

	if (!_macro) {
		return;
	}

	_pauseToggle->setText(_macro->Paused() ? _unpauseButtonText.c_str()
					       : _pauseButtonText.c_str());
	_statusText->setText(_macro->Matched() ? _conditionsTrueText.c_str()
					       : _conditionsFalseText.c_str());
}

} // namespace advss
