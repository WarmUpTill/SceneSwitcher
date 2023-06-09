#pragma once
#include "obs-dock.hpp"
#include "variable-string.hpp"

#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <memory>

namespace advss {

class Macro;

class MacroDock : public OBSDock {
	Q_OBJECT

public:
	MacroDock(Macro *, QWidget *parent, const StringVariable &runButtonText,
		  const StringVariable &pauseButtonText,
		  const StringVariable &unpauseButtonText,
		  const StringVariable &conditionsTrueText,
		  const StringVariable &conditionsFalseText);
	void SetName(const QString &);
	void ShowRunButton(bool);
	void SetRunButtonText(const StringVariable &);
	void ShowPauseButton(bool);
	void SetPauseButtonText(const StringVariable &);
	void SetUnpauseButtonText(const StringVariable &);
	void ShowStatusLabel(bool);
	void SetConditionsTrueText(const StringVariable &);
	void SetConditionsFalseText(const StringVariable &);

private slots:
	void RunClicked();
	void PauseToggleClicked();
	void UpdateText();

private:
	StringVariable _runButtonText;
	StringVariable _pauseButtonText;
	StringVariable _unpauseButtonText;
	StringVariable _conditionsTrueText;
	StringVariable _conditionsFalseText;
	QPushButton *_run;
	QPushButton *_pauseToggle;
	QLabel *_statusText;

	QTimer _timer;

	Macro *_macro;
};

} // namespace advss
