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
	PAGE_WINDOW_SCENE,
	PAGE_WINDOW_CONDITION,
	PAGE_WINDOW_REVIEW,
	PAGE_DONE,
};

// ---------------------------------------------------------------------------
// WelcomePage
// ---------------------------------------------------------------------------
class WelcomePage : public QWizardPage {
	Q_OBJECT
public:
	explicit WelcomePage(QWidget *parent = nullptr);
	int nextId() const override { return PAGE_WINDOW_SCENE; }
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
