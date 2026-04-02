#pragma once
#include "source-interaction-step.hpp"

#include <QComboBox>
#include <QStackedWidget>
#include <QWidget>

namespace advss {

class SourceInteractionStepEdit : public QWidget {
	Q_OBJECT
public:
	SourceInteractionStepEdit(QWidget *parent,
				  const SourceInteractionStep &step);
	SourceInteractionStep GetStep() const { return _step; }

signals:
	void StepChanged(const SourceInteractionStep &);

private slots:
	void TypeChanged(int);
	void UpdateStep();

private:
	void RebuildFields();

	QComboBox *_typeCombo;
	QStackedWidget *_fields;
	SourceInteractionStep _step;
};

} // namespace advss
