#pragma once

#include "duration-control.hpp"
#include "duration.hpp"
#include "first-run-wizard.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QWidget>

#include <memory>

class QVBoxLayout;

namespace advss {

namespace wiz {

// ---------------------------------------------------------------------------
// SequenceTriggerPage
//   Registers wizard field "seqTriggerScene" (QString).
//   Trigger duration is read back via GetTriggerDuration().
// ---------------------------------------------------------------------------
class SequenceTriggerPage : public QWizardPage {
	Q_OBJECT
public:
	explicit SequenceTriggerPage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_SEQ_SCENES; }

	Duration GetTriggerDuration() const;

private:
	QComboBox *_sceneCombo;
	DurationSelection *_delaySelection;
};

// ---------------------------------------------------------------------------
// SequenceScenesPage
//   Lets the user build an ordered list of scenes with delays between them.
//   The delay after the last scene is ignored when building the macro.
// ---------------------------------------------------------------------------
struct SequenceStep {
	QWidget *row;
	QComboBox *scene;
	QWidget *delayWidget; // hidden for the last row
	DurationSelection *delay;
	QPushButton *remove;
};

class SequenceScenesPage : public QWizardPage {
	Q_OBJECT
public:
	explicit SequenceScenesPage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_SEQ_REVIEW; }

	// Returns (sceneName, delayAfter) pairs. The last entry's delayAfter
	// is always a default-constructed Duration and must be ignored by the caller.
	QVector<QPair<QString, Duration>> GetSteps() const;

private slots:
	void onAddStepClicked();

private:
	void AddStep(int defaultSceneIndex = 0);
	void RemoveStep(QWidget *rowWidget);
	void UpdateDelayVisibility();
	void UpdateRemoveButtons();

	QLabel *_triggerInfoLabel;
	QWidget *_stepsContainer;
	QVBoxLayout *_stepsLayout;
	QVector<SequenceStep> _rows;
	bool _initialized = false;
};

// ---------------------------------------------------------------------------
// SequenceReviewPage
//   Displays a summary and builds the macro from wizard fields on Finish.
// ---------------------------------------------------------------------------
class SequenceReviewPage : public QWizardPage {
	Q_OBJECT
public:
	explicit SequenceReviewPage(QWidget *parent,
				    std::shared_ptr<advss::Macro> &macro);
	void initializePage() override;
	bool validatePage() override;
	int nextId() const override { return PAGE_DONE; }

private:
	QLabel *_summary;
	std::shared_ptr<advss::Macro> &_macro;
};

} // namespace wiz
} // namespace advss
