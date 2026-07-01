#pragma once

#include <QRadioButton>
#include <QWizard>
#include <QWizardPage>

#include <memory>

namespace advss {

bool IsFirstRun();

class Macro;

namespace wiz {

// ---------------------------------------------------------------------------
// Page IDs
// ---------------------------------------------------------------------------
enum WizardPageId {
	PAGE_WELCOME = 0,
	PAGE_TEMPLATE,
	PAGE_WINDOW_SCENE,
	PAGE_WINDOW_CONDITION,
	PAGE_WINDOW_REVIEW,
	PAGE_SEQ_TRIGGER,
	PAGE_SEQ_SCENES,
	PAGE_SEQ_REVIEW,
	PAGE_AUDIO_SOURCE,
	PAGE_AUDIO_TARGET,
	PAGE_AUDIO_REVIEW,
	PAGE_DONE,
};

// ---------------------------------------------------------------------------
// WelcomePage
// ---------------------------------------------------------------------------
class WelcomePage : public QWizardPage {
	Q_OBJECT
public:
	explicit WelcomePage(QWidget *parent = nullptr);
	int nextId() const override { return PAGE_TEMPLATE; }
};

// ---------------------------------------------------------------------------
// TemplatePage
//   Lets the user choose which kind of automation to create.
// ---------------------------------------------------------------------------
class TemplatePage : public QWizardPage {
	Q_OBJECT
public:
	explicit TemplatePage(QWidget *parent = nullptr);
	int nextId() const override;

private:
	QRadioButton *_windowRadio;
	QRadioButton *_sequenceRadio;
	QRadioButton *_audioRadio;
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
	static std::shared_ptr<advss::Macro>
	ShowWizard(QWidget *parent, bool *wasSkipped = nullptr);

private:
	void markFirstRunComplete();

	std::shared_ptr<advss::Macro> _macro;
};

} // namespace wiz
} // namespace advss
