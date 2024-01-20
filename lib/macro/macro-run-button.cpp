#include "macro-run-button.hpp"
#include "obs-module-helper.hpp"
#include "macro-tree.hpp"
#include "macro.hpp"
#include "ui-helpers.hpp"

#include <QKeyEvent>

namespace advss {

MacroRunButton::MacroRunButton(QWidget *parent) : QPushButton(parent)
{
	installEventFilter(this);
	auto parentWindow = window();
	if (parentWindow) {
		parentWindow->installEventFilter(this);
	}

	setToolTip(obs_module_text("AdvSceneSwitcher.macroTab.run.tooltip"));

	QWidget::connect(this, SIGNAL(pressed()), this, SLOT(Pressed()));
}

void MacroRunButton::SetMacroTree(MacroTree *macros)
{
	_macros = macros;

	QWidget::connect(macros, SIGNAL(MacroSelectionChanged()), this,
			 SLOT(MacroSelectionChanged()));
	QWidget::connect(&_timer, &QTimer::timeout, this,
			 [this]() { MacroSelectionChanged(); });

	_timer.start(1000);
}

void MacroRunButton::MacroSelectionChanged()
{
	auto macro = _macros->GetCurrentMacro();
	if (!macro) {
		_macroHasElseActions = false;
		return;
	}

	_macroHasElseActions = macro->ElseActions().size() > 0;
}

bool MacroRunButton::eventFilter(QObject *obj, QEvent *event)
{
	if (!_macroHasElseActions) {
		DeactivateElseState();
		return QPushButton::eventFilter(obj, event);
	}

	auto eventType = event->type();
	if (eventType == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Shift) {
			if (underMouse()) {
				ActivateElseState();
			}
			_shiftHeld = true;
		}
	} else if (eventType == QEvent::KeyRelease) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Shift) {
			DeactivateElseState();
			_shiftHeld = false;
		}
	} else if (_shiftHeld && obj == this && eventType == QEvent::Enter) {
		ActivateElseState();
	} else if (obj == this && eventType == QEvent::Leave) {
		DeactivateElseState();
	}

	return QPushButton::eventFilter(obj, event);
}

void MacroRunButton::ActivateElseState()
{
	setText(obs_module_text("AdvSceneSwitcher.macroTab.runElse"));
	_elseStateActive = true;
}

void MacroRunButton::DeactivateElseState()
{
	setText(obs_module_text("AdvSceneSwitcher.macroTab.run"));
	_elseStateActive = false;
}

void MacroRunButton::Pressed()
{
	auto macro = _macros->GetCurrentMacro();
	if (!macro) {
		return;
	}

	bool ret = _elseStateActive ? macro->PerformActions(false, true, true)
				    : macro->PerformActions(true, true, true);

	if (!ret) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroTab.runFail");
		DisplayMessage(err.arg(QString::fromStdString(macro->Name())));
	}
}

} // namespace advss
