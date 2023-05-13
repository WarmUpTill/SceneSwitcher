#pragma once
#include "obs-dock.hpp"

#include <QPushButton>
#include <QTimer>
#include <memory>

namespace advss {

class Macro;

class MacroDock : public OBSDock {
	Q_OBJECT

public:
	MacroDock(Macro *, QWidget *parent, const QString &runButtonText,
		  const QString &pauseButtonText,
		  const QString &unpauseButtonText);
	void SetName(const QString &);
	void ShowRunButton(bool);
	void SetRunButtonText(const QString &);
	void ShowPauseButton(bool);
	void SetPauseButtonText(const QString &);
	void SetUnpauseButtonText(const QString &);

private slots:
	void RunClicked();
	void PauseToggleClicked();
	void UpdatePauseText();

private:
	QString _pauseButtonText;
	QString _unpauseButtonText;
	QPushButton *_run;
	QPushButton *_pauseToggle;

	QTimer _timer;

	Macro *_macro;
};

} // namespace advss
