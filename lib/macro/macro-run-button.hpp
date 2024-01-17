#pragma once
#include <QPushButton>
#include <QTimer>

namespace advss {

class MacroTree;

class MacroRunButton : public QPushButton {
	Q_OBJECT
public:
	MacroRunButton(QWidget *parent = nullptr);
	void SetMacroTree(MacroTree *);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
	void MacroSelectionChanged();
	void Pressed();

private:
	void ActivateElseState();
	void DeactivateElseState();

	bool _macroHasElseActions = false;
	bool _elseStateActive = false;
	bool _shiftHeld = false;
	MacroTree *_macros = nullptr;
	QTimer _timer;
};

} // namespace advss
