#pragma once

#include <obs-data.h>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QWizard>
#include <QWizardPage>

#include <string>

namespace advss {

class Macro;

bool IsFirstRun();

// ---------------------------------------------------------------------------
// Page IDs
// ---------------------------------------------------------------------------
enum WizardPageId {
	PAGE_WELCOME = 0,
	PAGE_SCENE,
	PAGE_WINDOW,
	PAGE_REVIEW,
	PAGE_DONE,
};

// ---------------------------------------------------------------------------
// WelcomePage
// ---------------------------------------------------------------------------
class WelcomePage : public QWizardPage {
	Q_OBJECT
public:
	explicit WelcomePage(QWidget *parent = nullptr);
	int nextId() const override { return PAGE_SCENE; }
};

// ---------------------------------------------------------------------------
// SceneSelectionPage
//   Registers wizard field "targetScene" (QString).
// ---------------------------------------------------------------------------
class SceneSelectionPage : public QWizardPage {
	Q_OBJECT
public:
	explicit SceneSelectionPage(QWidget *parent = nullptr);
	void initializePage() override;
	bool isComplete() const override;
	int nextId() const override { return PAGE_WINDOW; }

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
	int nextId() const override { return PAGE_REVIEW; }

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
// ReviewPage
//   Displays a summary and calls FirstRunWizard::CreateMacro() on Finish.
// ---------------------------------------------------------------------------
class ReviewPage : public QWizardPage {
	Q_OBJECT
public:
	explicit ReviewPage(QWidget *parent, std::shared_ptr<Macro> &macro);
	void initializePage() override;
	bool validatePage() override;
	int nextId() const override { return PAGE_DONE; }

private:
	QLabel *_summary;
	std::shared_ptr<Macro> &_macro;
};

// ---------------------------------------------------------------------------
// DonePage
// ---------------------------------------------------------------------------
class DonePage : public QWizardPage {
	Q_OBJECT
public:
	explicit DonePage(QWidget *parent = nullptr);
	int nextId() const override { return -1; }
};

// ---------------------------------------------------------------------------
// FirstRunWizard
// ---------------------------------------------------------------------------
class FirstRunWizard : public QWizard {
	Q_OBJECT
public:
	explicit FirstRunWizard(QWidget *parent = nullptr);
	static std::shared_ptr<Macro> ShowWizard(QWidget *parent,
						 bool *wasSkipped = nullptr);
	static bool
	CreateMacro(std::shared_ptr<Macro> &macro, const std::string &macroName,
		    const std::string &conditionId, obs_data_t *conditionData,
		    const std::string &actionId, obs_data_t *actionData);

private:
	void markFirstRunComplete();

	std::shared_ptr<Macro> _macro;
};

} // namespace advss
