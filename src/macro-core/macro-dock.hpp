#pragma once
#include "obs-dock.hpp"

#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <memory>

namespace advss {

class Macro;

class MacroDock : public OBSDock {
	Q_OBJECT

public:
	MacroDock(Macro *, QWidget *parent, const QString &runButtonText,
		  const QString &pauseButtonText,
		  const QString &unpauseButtonText,
		  const QString &conditionsTrueText,
		  const QString &conditionsFalseText);
	void SetName(const QString &);
	void ShowRunButton(bool);
	void SetRunButtonText(const QString &);
	void ShowPauseButton(bool);
	void SetPauseButtonText(const QString &);
	void SetUnpauseButtonText(const QString &);
	void ShowStatusLabel(bool);
	void SetConditionsTrueText(const QString &);
	void SetConditionsFalseText(const QString &);

private slots:
	void RunClicked();
	void PauseToggleClicked();
	void UpdatePauseText();
	void UpdateStatusText();

private:
	QString _pauseButtonText;
	QString _unpauseButtonText;
	QString _conditionsTrueText;
	QString _conditionsFalseText;
	QPushButton *_run;
	QPushButton *_pauseToggle;
	QLabel *_statusText;

	QTimer _timer;

	Macro *_macro;
};

} // namespace advss
