#include "macro-dock.hpp"
#include "macro.hpp"
#include "ui-helpers.hpp"

#include <QLayout>

namespace advss {

MacroDock::MacroDock(std::weak_ptr<Macro> m,
		     const StringVariable &runButtonText,
		     const StringVariable &pauseButtonText,
		     const StringVariable &unpauseButtonText,
		     const StringVariable &conditionsTrueText,
		     const StringVariable &conditionsFalseText,
		     bool enableHighlight)
	: QFrame(),
	  _runButtonText(runButtonText),
	  _pauseButtonText(pauseButtonText),
	  _unpauseButtonText(unpauseButtonText),
	  _conditionsTrueText(conditionsTrueText),
	  _conditionsFalseText(conditionsFalseText),
	  _highlight(enableHighlight),
	  _run(new QPushButton(runButtonText.c_str())),
	  _pauseToggle(new QPushButton()),
	  _statusText(new QLabel(conditionsFalseText.c_str())),
	  _macro(m)
{
	setFrameShape(QFrame::StyledPanel);
	setFrameShadow(QFrame::Sunken);

	auto macro = _macro.lock();
	if (macro) {
		const auto &settings = macro->GetDockSettings();
		_run->setVisible(settings.HasRunButton());
		_pauseToggle->setVisible(settings.HasPauseButton());
		_statusText->setVisible(settings.HasStatusLabel());
	}

	QWidget::connect(_run, SIGNAL(clicked()), this, SLOT(RunClicked()));
	QWidget::connect(_pauseToggle, SIGNAL(clicked()), this,
			 SLOT(PauseToggleClicked()));

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_run);
	layout->addWidget(_pauseToggle);
	layout->addWidget(_statusText);

	UpdateText();
	QWidget::connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdateText()));
	QWidget::connect(&_timer, SIGNAL(timeout()), this, SLOT(Highlight()));
	_timer.start(500);

	setLayout(layout);
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

void MacroDock::EnableHighlight(bool value)
{
	_highlight = value;
}

void MacroDock::RunClicked()
{
	auto macro = _macro.lock();
	if (!macro) {
		return;
	}

	auto ret = macro->PerformActions(true, true, true);
	if (!ret) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroTab.runFail");
		DisplayMessage(err.arg(QString::fromStdString(macro->Name())));
	}
}

void MacroDock::PauseToggleClicked()
{
	auto macro = _macro.lock();
	if (!macro) {
		return;
	}

	macro->SetPaused(!macro->Paused());
	UpdateText();
}

void MacroDock::UpdateText()
{
	_run->setText(_runButtonText.c_str());
	auto macro = _macro.lock();
	if (!macro) {
		return;
	}

	_pauseToggle->setText(macro->Paused() ? _unpauseButtonText.c_str()
					      : _pauseButtonText.c_str());
	_statusText->setText(macro->ConditionsMatched()
				     ? _conditionsTrueText.c_str()
				     : _conditionsFalseText.c_str());
}

void MacroDock::Highlight()
{
	auto macro = _macro.lock();
	if (!_highlight || !macro) {
		return;
	}
	if (_lastHighlightCheckTime.time_since_epoch().count() != 0 &&
	    macro->WasExecutedSince(_lastHighlightCheckTime)) {
		HighlightWidget(this, Qt::green, QColor(0, 0, 0, 0), true);
	}
	_lastHighlightCheckTime = std::chrono::high_resolution_clock::now();
}

} // namespace advss
