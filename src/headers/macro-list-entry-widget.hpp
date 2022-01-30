#pragma once

#include <QLabel>
#include <QCheckBox>
#include <memory>

class Macro;

class MacroListEntryWidget : public QWidget {
	Q_OBJECT

public:
	MacroListEntryWidget(std::shared_ptr<Macro>, QWidget *parent);
	void SetName(const QString &);
	void SetMacro(std::shared_ptr<Macro> &);

private slots:
	void PauseChanged(int);

private:
	QLabel *_name;
	QCheckBox *_running;
	std::shared_ptr<Macro> _macro;
};
