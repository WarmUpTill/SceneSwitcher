#include "macro-run-button.hpp"
#include "macro-tree.hpp"
#include "macro.hpp"
#include "obs-module-helper.hpp"
#include "utility.hpp"

#include <QKeyEvent>

namespace advss {

MacroRunButton::MacroRunButton(QWidget *parent) : QPushButton(parent)
{
	if (window()) {
		window()->installEventFilter(this);
	}
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
		setText(obs_module_text("AdvSceneSwitcher.macroTab.run"));
		_runElseActionsKeyHeld = false;
		return QPushButton::eventFilter(obj, event);
	}

	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Control) {
			setText(obs_module_text(
				"AdvSceneSwitcher.macroTab.runElse"));
			_runElseActionsKeyHeld = true;
		}
	} else if (event->type() == QEvent::KeyRelease) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Control) {
			setText(obs_module_text(
				"AdvSceneSwitcher.macroTab.run"));
			_runElseActionsKeyHeld = false;
		}
	}
	return QPushButton::eventFilter(obj, event);
}

void MacroRunButton::Pressed()
{
	auto macro = _macros->GetCurrentMacro();
	if (!macro) {
		return;
	}

	bool ret = _runElseActionsKeyHeld
			   ? macro->PerformActions(false, true, true)
			   : macro->PerformActions(true, true, true);
	if (!ret) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroTab.runFail");
		DisplayMessage(err.arg(QString::fromStdString(macro->Name())));
	}
}

} // namespace advss
