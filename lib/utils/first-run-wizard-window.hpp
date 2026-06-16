#pragma once

#include "first-run-wizard.hpp"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

#include <memory>

namespace advss {

namespace wiz {

// ---------------------------------------------------------------------------
// WindowSceneSelectionPage
//   Registers wizard field "targetScene" (QString).
// ---------------------------------------------------------------------------
class WindowSceneSelectionPage : public QWizardPage {
	Q_OBJECT
public:
	explicit WindowSceneSelectionPage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_WINDOW_CONDITION; }

private:
	QComboBox *_sceneCombo;
};

// ---------------------------------------------------------------------------
// WindowConditionPage
//   Registers wizard field "windowTitle" (QString).
//   Auto-detect button samples the focused window after a countdown.
// ---------------------------------------------------------------------------
class WindowConditionPage : public QWizardPage {
	Q_OBJECT
public:
	explicit WindowConditionPage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_WINDOW_REVIEW; }

private slots:
	void onAutoDetectClicked();
	void onCountdownTick();

private:
	QLineEdit *_windowEdit;
	QPushButton *_autoDetect;
	QTimer *_detectTimer;
	int _countdown = 3;
};

// ---------------------------------------------------------------------------
// WindowReviewPage
//   Displays a summary and builds the macro from wizard fields on Finish.
// ---------------------------------------------------------------------------
class WindowReviewPage : public QWizardPage {
	Q_OBJECT
public:
	explicit WindowReviewPage(QWidget *parent,
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
