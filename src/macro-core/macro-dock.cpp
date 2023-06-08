#include "macro-dock.hpp"
#include "macro.hpp"
#include "utility.hpp"

#include <QLayout>

namespace advss {

MacroDock::MacroDock(Macro *m, QWidget *parent, const QString &runButtonText,
		     const QString &pauseButtonText,
		     const QString &unpauseButtonText,
		     const QString &conditionsTrueText,
		     const QString &conditionsFalseText)
	: OBSDock(parent),
	  _pauseButtonText(pauseButtonText),
	  _unpauseButtonText(unpauseButtonText),
	  _conditionsTrueText(conditionsTrueText),
	  _conditionsFalseText(conditionsFalseText),
	  _run(new QPushButton(runButtonText)),
	  _pauseToggle(new QPushButton()),
	  _statusText(new QLabel(conditionsFalseText)),
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

	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdatePauseText()));
	QWidget::connect(&_timer, SIGNAL(timeout()), this,
			 SLOT(UpdateStatusText()));
	_timer.start(1000);

	UpdatePauseText();
	UpdateStatusText();

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

void MacroDock::SetRunButtonText(const QString &text)
{
	_run->setText(text);
}

void MacroDock::ShowPauseButton(bool value)
{
	_pauseToggle->setVisible(value);
}

void MacroDock::SetPauseButtonText(const QString &text)
{
	_pauseButtonText = text;
	UpdatePauseText();
}

void MacroDock::SetUnpauseButtonText(const QString &text)
{
	_unpauseButtonText = text;
	UpdatePauseText();
}

void MacroDock::ShowStatusLabel(bool value)
{
	_statusText->setVisible(value);
}

void MacroDock::SetConditionsTrueText(const QString &text)
{
	_conditionsTrueText = text;
	UpdateStatusText();
}

void MacroDock::SetConditionsFalseText(const QString &text)
{
	_conditionsFalseText = text;
	UpdateStatusText();
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

	_pauseToggle->setText(_macro->Paused() ? _unpauseButtonText
					       : _pauseButtonText);
}

void MacroDock::UpdateStatusText()
{
	if (!_macro) {
		return;
	}

	_statusText->setText(_macro->Matched() ? _conditionsTrueText
					       : _conditionsFalseText);
}

} // namespace advss
