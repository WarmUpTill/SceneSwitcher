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
	MacroTree *_macros = nullptr;
	bool _macroHasElseActions = false;
	bool _runElseActionsKeyHeld = false;
	QTimer _timer;
};

} // namespace advss
