#pragma once

#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <memory>

class Macro;

class MacroListEntryWidget : public QWidget {
	Q_OBJECT

public:
	MacroListEntryWidget(std::shared_ptr<Macro>, bool highlight,
			     QWidget *parent);
	void SetName(const QString &);
	void SetMacro(std::shared_ptr<Macro> &);

private slots:
	void PauseChanged(int);
	void HighlightExecuted();
	void UpdatePaused();
	void EnableHighlight(bool);

private:
	QTimer _timer;
	QLabel *_name;
	QCheckBox *_running;
	std::shared_ptr<Macro> _macro;

	bool _highlightExecutedMacros = false;
};
