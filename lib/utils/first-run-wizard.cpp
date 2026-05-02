#include "first-run-wizard.hpp"

#include "log-helper.hpp"
#include "macro.hpp"
#include "macro-action-factory.hpp"
#include "macro-condition-factory.hpp"
#include "macro-settings.hpp"
#include "platform-funcs.hpp"
#include "selection-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs-data.h>
#include <util/config-file.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace advss {

static constexpr char kConditionIdWindow[] = "window";
static constexpr char kActionIdSceneSwitch[] = "scene_switch";

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

static QString DetectFocusedWindow()
{
	return QString::fromStdString(GetCurrentWindowTitle());
}

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
// SceneSelectionPage
// ===========================================================================

SceneSelectionPage::SceneSelectionPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(obs_module_text("FirstRunWizard.scene.title"));
	setSubTitle(obs_module_text("FirstRunWizard.scene.subtitle"));

	auto label =
		new QLabel(obs_module_text("FirstRunWizard.scene.label"), this);
	_sceneCombo = new QComboBox(this);
	_sceneCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	// registerField with * suffix means the field is mandatory for Next
	registerField("targetScene*", _sceneCombo, "currentText",
		      SIGNAL(currentTextChanged(QString)));

	connect(_sceneCombo, &QComboBox::currentTextChanged, this,
		&QWizardPage::completeChanged);

	auto row = new QHBoxLayout;
	row->addWidget(label);
	row->addWidget(_sceneCombo, 1);

	auto layout = new QVBoxLayout(this);
	layout->addLayout(row);
	layout->addStretch();
}

void SceneSelectionPage::initializePage()
{
	_sceneCombo->clear();
	for (const QString &name : GetSceneNames())
		_sceneCombo->addItem(name);
}

bool SceneSelectionPage::isComplete() const
{
	return _sceneCombo->count() > 0 &&
	       !_sceneCombo->currentText().isEmpty();
}

// ===========================================================================
// WindowConditionPage
// ===========================================================================

WindowConditionPage::WindowConditionPage(QWidget *parent)
	: QWizardPage(parent),
	  _detectTimer(new QTimer(this))
{
	setTitle(obs_module_text("FirstRunWizard.window.title"));
	setSubTitle(obs_module_text("FirstRunWizard.window.subtitle"));

	auto label = new QLabel(obs_module_text("FirstRunWizard.window.label"),
				this);
	_windowEdit = new QLineEdit(this);
	_windowEdit->setPlaceholderText(
		obs_module_text("FirstRunWizard.window.placeholder"));

	_autoDetect = new QPushButton(
		obs_module_text("FirstRunWizard.window.autoDetect"), this);
	_autoDetect->setToolTip(
		obs_module_text("FirstRunWizard.window.autoDetectTooltip"));

	registerField("windowTitle*", _windowEdit);

	connect(_windowEdit, &QLineEdit::textChanged, this,
		&QWizardPage::completeChanged);
	connect(_autoDetect, &QPushButton::clicked, this,
		&WindowConditionPage::onAutoDetectClicked);
	connect(_detectTimer, &QTimer::timeout, this,
		&WindowConditionPage::onCountdownTick);

	auto row = new QHBoxLayout;
	row->addWidget(label);
	row->addWidget(_windowEdit, 1);

	auto hint =
		new QLabel(obs_module_text("FirstRunWizard.window.hint"), this);
	hint->setTextFormat(Qt::RichText);
	hint->setWordWrap(true);

	auto layout = new QVBoxLayout(this);
	layout->addLayout(row);
	layout->addWidget(_autoDetect, 0, Qt::AlignLeft);
	layout->addWidget(hint);
	layout->addStretch();
}

void WindowConditionPage::initializePage()
{
	if (_windowEdit->text().isEmpty()) {
		QString detected = DetectFocusedWindow();
		if (!detected.isEmpty()) {
			_windowEdit->setText(detected);
		}
	}
}

bool WindowConditionPage::isComplete() const
{
	return !_windowEdit->text().trimmed().isEmpty();
}

void WindowConditionPage::onAutoDetectClicked()
{
	_countdown = 3;
	_autoDetect->setEnabled(false);
	_autoDetect->setText(
		QString(obs_module_text(
				"FirstRunWizard.window.autoDetectCountdown"))
			.arg(_countdown));
	_detectTimer->start(1000);
}

void WindowConditionPage::onCountdownTick()
{
	--_countdown;
	if (_countdown > 0) {
		_autoDetect->setText(
			QString(obs_module_text(
					"FirstRunWizard.window.autoDetectCountdown"))
				.arg(_countdown));
		return;
	}

	_detectTimer->stop();
	QString title = DetectFocusedWindow();
	if (!title.isEmpty()) {
		_windowEdit->setText(title);
	}

	_autoDetect->setEnabled(true);
	_autoDetect->setText(
		obs_module_text("FirstRunWizard.window.autoDetect"));
}

// ===========================================================================
// ReviewPage
// ===========================================================================

ReviewPage::ReviewPage(QWidget *parent, std::shared_ptr<Macro> &macro)
	: QWizardPage(parent),
	  _macro(macro)
{
	setTitle(obs_module_text("FirstRunWizard.review.title"));
	setSubTitle(obs_module_text("FirstRunWizard.review.subtitle"));

	_summary = new QLabel(this);
	_summary->setWordWrap(true);
	_summary->setTextFormat(Qt::RichText);
	_summary->setFrameShape(QFrame::StyledPanel);
	_summary->setContentsMargins(12, 12, 12, 12);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(_summary);
	layout->addStretch();
}

void ReviewPage::initializePage()
{
	const QString scene = field("targetScene").toString();
	const QString window = field("windowTitle").toString();

	_summary->setText(
		QString(obs_module_text("FirstRunWizard.review.summary"))
			.arg(scene.toHtmlEscaped(), window.toHtmlEscaped()));
}

static QString escapeForRegex(const QString &input)
{
	return QRegularExpression::escape(input);
}

bool ReviewPage::validatePage()
{
	const QString scene = field("targetScene").toString();
	const QString window = escapeForRegex(field("windowTitle").toString());
	const std::string name = ("Window -> " + scene).toStdString();

	// Build condition data blob
	// ---------------------------------------------------------------
	// Condition blob — mirrors MacroConditionWindow::Save() output:
	//
	// {
	//     "segmentSettings": { "enabled": true, "version": 1 },
	//     "id": "window",
	//     "checkTitle": true,
	//     "window": "<user input>",
	//     "windowRegexConfig": {
	//         "enable": true,   // use regex-style partial matching
	//         "partial": true,  // match anywhere in the title
	//         "options": 3      // case-insensitive (QRegularExpression flags)
	//     },
	//     "focus": true,        // only trigger when window is focused
	//     "version": 1
	// }
	// ---------------------------------------------------------------
	OBSDataAutoRelease condSegment = obs_data_create();
	obs_data_set_bool(condSegment, "enabled", true);
	obs_data_set_int(condSegment, "version", 1);

	OBSDataAutoRelease condRegex = obs_data_create();
	obs_data_set_bool(condRegex, "enable", true);
	obs_data_set_bool(condRegex, "partial", true);
	obs_data_set_int(condRegex, "options", 3); // CaseInsensitiveOption

	OBSDataAutoRelease condData = obs_data_create();
	obs_data_set_obj(condData, "segmentSettings", condSegment);
	obs_data_set_string(condData, "id", "window");
	obs_data_set_bool(condData, "checkTitle", true);
	obs_data_set_string(condData, "window", window.toUtf8().constData());
	obs_data_set_obj(condData, "windowRegexConfig", condRegex);
	obs_data_set_bool(condData, "focus", true);
	obs_data_set_int(condData, "version", 1);

	// Build action data blob
	// ---------------------------------------------------------------
	// Action blob — mirrors MacroActionSwitchScene::Save() output:
	//
	// {
	//     "segmentSettings": { "enabled": true, "version": 1 },
	//     "id": "scene_switch",
	//     "action": 0,              // 0 = switch scene
	//     "sceneSelection": {
	//         "type": 0,            // 0 = scene by name
	//         "name": "<scene>",
	//         "canvasSelection": "Main"
	//     },
	//     "transitionType": 1,      // 1 = use scene's default transition
	//     "blockUntilTransitionDone": false,
	//     "sceneType": 0
	// }
	// ---------------------------------------------------------------
	OBSDataAutoRelease actionSegment = obs_data_create();
	obs_data_set_bool(actionSegment, "enabled", true);
	obs_data_set_int(actionSegment, "version", 1);

	OBSDataAutoRelease sceneSelection = obs_data_create();
	obs_data_set_int(sceneSelection, "type", 0);
	obs_data_set_string(sceneSelection, "name", scene.toUtf8().constData());
	obs_data_set_string(sceneSelection, "canvasSelection", "Main");

	OBSDataAutoRelease actionData = obs_data_create();
	obs_data_set_obj(actionData, "segmentSettings", actionSegment);
	obs_data_set_string(actionData, "id", "scene_switch");
	obs_data_set_int(actionData, "action", 0);
	obs_data_set_obj(actionData, "sceneSelection", sceneSelection);
	obs_data_set_int(actionData, "transitionType", 1);
	obs_data_set_bool(actionData, "blockUntilTransitionDone", false);
	obs_data_set_int(actionData, "sceneType", 0);

	if (!FirstRunWizard::CreateMacro(_macro, name, kConditionIdWindow,
					 condData, kActionIdSceneSwitch,
					 actionData)) {
		QMessageBox::warning(
			this,
			obs_module_text("FirstRunWizard.review.errorTitle"),
			QString(obs_module_text(
					"FirstRunWizard.review.errorBody"))
				.arg(window, scene));
		_macro.reset();
		// Still advance so the user is not stuck.
	}
	return true;
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
	setPage(PAGE_SCENE, new SceneSelectionPage(this));
	setPage(PAGE_WINDOW, new WindowConditionPage(this));
	setPage(PAGE_REVIEW, new ReviewPage(this, _macro));
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

// static
bool FirstRunWizard::CreateMacro(std::shared_ptr<Macro> &macro,
				 const std::string &macroName,
				 const std::string &conditionId,
				 obs_data_t *conditionData,
				 const std::string &actionId,
				 obs_data_t *actionData)
{
	// 1. Create and register the Macro
	macro = std::make_shared<Macro>(macroName, GetGlobalMacroSettings());
	if (!macro) {
		blog(LOG_WARNING, "FirstRunWizard: Macro allocation failed");
		return false;
	}

	// 2. Instantiate condition via factory, then hydrate via Load()
	auto condition =
		MacroConditionFactory::Create(conditionId, macro.get());
	if (!condition) {
		blog(LOG_WARNING,
		     "FirstRunWizard: condition factory returned null "
		     "for id '%s' — is the base plugin loaded?",
		     conditionId.c_str());
		return false;
	}
	if (!condition->Load(conditionData)) {
		blog(LOG_WARNING,
		     "FirstRunWizard: condition Load() failed for id '%s'",
		     conditionId.c_str());
		return false;
	}
	macro->Conditions().emplace_back(condition);

	// 3. Instantiate action via factory, then hydrate via Load()
	auto action = MacroActionFactory::Create(actionId, macro.get());
	if (!action) {
		blog(LOG_WARNING,
		     "FirstRunWizard: action factory returned null "
		     "for id '%s' — is the base plugin loaded?",
		     actionId.c_str());
		return false;
	}
	if (!action->Load(actionData)) {
		blog(LOG_WARNING,
		     "FirstRunWizard: action Load() failed for id '%s'",
		     actionId.c_str());
		return false;
	}
	macro->Actions().emplace_back(action);

	blog(LOG_INFO, "FirstRunWizard: created macro '%s'", macroName.c_str());
	return true;
}

} // namespace advss
