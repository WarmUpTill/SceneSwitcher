#pragma once
#include "variable-string.hpp"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <memory>
#include <chrono>

namespace advss {

class Macro;

class MacroDock : public QFrame {
	Q_OBJECT

public:
	MacroDock(std::weak_ptr<Macro>, const StringVariable &runButtonText,
		  const StringVariable &pauseButtonText,
		  const StringVariable &unpauseButtonText,
		  const StringVariable &conditionsTrueText,
		  const StringVariable &conditionsFalseText,
		  bool enableHighlight);
	void ShowRunButton(bool);
	void SetRunButtonText(const StringVariable &);
	void ShowPauseButton(bool);
	void SetPauseButtonText(const StringVariable &);
	void SetUnpauseButtonText(const StringVariable &);
	void ShowStatusLabel(bool);
	void SetConditionsTrueText(const StringVariable &);
	void SetConditionsFalseText(const StringVariable &);
	void EnableHighlight(bool);

private slots:
	void RunClicked();
	void PauseToggleClicked();
	void UpdateText();
	void Highlight();

private:
	StringVariable _runButtonText;
	StringVariable _pauseButtonText;
	StringVariable _unpauseButtonText;
	StringVariable _conditionsTrueText;
	StringVariable _conditionsFalseText;
	bool _highlight;
	QPushButton *_run;
	QPushButton *_pauseToggle;
	QLabel *_statusText;

	QTimer _timer;
	std::chrono::high_resolution_clock::time_point _lastHighlightCheckTime{};

	std::weak_ptr<Macro> _macro;
};

} // namespace advss
