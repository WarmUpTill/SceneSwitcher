#include "first-run-wizard.hpp"
#include "first-run-wizard-window.hpp"

#include "macro.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QVBoxLayout>

namespace advss {

// ---------------------------------------------------------------------------
// OBS global config helpers
// ---------------------------------------------------------------------------
static constexpr char kConfigSection[] = "AdvancedSceneSwitcher";
static constexpr char kFirstRunKey[] = "firstRun";

bool IsFirstRun()
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(31, 0, 0)
	config_t *cfg = obs_frontend_get_user_config();
#else
	config_t *cfg = obs_frontend_get_global_config();
#endif
	if (!config_has_user_value(cfg, kConfigSection, kFirstRunKey)) {
		return true;
	}
	return config_get_bool(cfg, kConfigSection, kFirstRunKey);
}

static void WriteFirstRun(bool value)
{
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(31, 0, 0)
	config_t *cfg = obs_frontend_get_user_config();
#else
	config_t *cfg = obs_frontend_get_global_config();
#endif
	config_set_bool(cfg, kConfigSection, kFirstRunKey, value);
	config_save_safe(cfg, "tmp", nullptr);
}

} // namespace advss

namespace advss {
namespace wiz {

// ===========================================================================
// WelcomePage
// ===========================================================================

WelcomePage::WelcomePage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(obs_module_text("FirstRunWizard.welcome.title"));
	setSubTitle(obs_module_text("FirstRunWizard.welcome.subtitle"));

	auto body = new QLabel(obs_module_text("FirstRunWizard.welcome.body"),
			       this);
	body->setWordWrap(true);
	body->setTextFormat(Qt::RichText);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(body);
	layout->addStretch();
}

// ===========================================================================
// DonePage
// ===========================================================================

DonePage::DonePage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(obs_module_text("FirstRunWizard.done.title"));
	setSubTitle(obs_module_text("FirstRunWizard.done.subtitle"));

	auto body =
		new QLabel(obs_module_text("FirstRunWizard.done.body"), this);
	body->setWordWrap(true);
	body->setTextFormat(Qt::RichText);
	body->setOpenExternalLinks(true);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(body);
	layout->addStretch();
}

// ===========================================================================
// FirstRunWizard
// ===========================================================================

FirstRunWizard::FirstRunWizard(QWidget *parent) : QWizard(parent)
{
	setWindowTitle(obs_module_text("FirstRunWizard.windowTitle"));
	setWizardStyle(QWizard::ModernStyle);
	setMinimumSize(540, 420);

	setPage(PAGE_WELCOME, new WelcomePage(this));
	setPage(PAGE_WINDOW_SCENE, new WindowSceneSelectionPage(this));
	setPage(PAGE_WINDOW_CONDITION, new WindowConditionPage(this));
	setPage(PAGE_WINDOW_REVIEW, new WindowReviewPage(this, _macro));
	setPage(PAGE_DONE, new DonePage(this));

	setStartId(PAGE_WELCOME);
	setOption(QWizard::NoBackButtonOnLastPage, true);
	setOption(QWizard::NoCancelButtonOnLastPage, true);

	// Mark done on both Accept (Finish) and Reject (Cancel / close)
	connect(this, &QWizard::accepted, this,
		&FirstRunWizard::markFirstRunComplete);
	connect(this, &QWizard::rejected, this,
		&FirstRunWizard::markFirstRunComplete);
}

void FirstRunWizard::markFirstRunComplete()
{
	WriteFirstRun(false);
}

// static
std::shared_ptr<Macro> FirstRunWizard::ShowWizard(QWidget *parent,
						  bool *wasSkipped)
{
	auto wizard = new FirstRunWizard(parent);
	wizard->exec();
	if (wasSkipped) {
		*wasSkipped = (wizard->result() == QDialog::Rejected);
	}
	wizard->deleteLater();
	return wizard->_macro;
}

} // namespace wiz
} // namespace advss
