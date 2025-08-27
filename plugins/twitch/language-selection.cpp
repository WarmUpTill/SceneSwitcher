#include "language-selection.hpp"
#include "channel-selection.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "twitch-helpers.hpp"
#include "ui-helpers.hpp"

#include <QPushButton>
#include <QVBoxLayout>

namespace advss {

namespace {

struct LocaleMapping {
	const QString languageCode;
	const QString name;
};

const std::vector<LocaleMapping> &getLocales()
{
	static std::vector<LocaleMapping> locales;
	static const char *lastLocaleCode = "";
	const char *curLocale = obs_get_locale();
	if (strcmp(lastLocaleCode, curLocale) != 0) {
		locales.clear();
		locales.emplace_back(LocaleMapping{
			"en",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.english")});
		locales.emplace_back(LocaleMapping{
			"id",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.indonesian")});
		locales.emplace_back(LocaleMapping{
			"ca",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.catalan")});
		locales.emplace_back(LocaleMapping{
			"da",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.danish")});
		locales.emplace_back(LocaleMapping{
			"de",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.german")});
		locales.emplace_back(LocaleMapping{
			"es",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.spanish")});
		locales.emplace_back(LocaleMapping{
			"fr",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.french")});
		locales.emplace_back(LocaleMapping{
			"it",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.italian")});
		locales.emplace_back(LocaleMapping{
			"hu",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.hungarian")});
		locales.emplace_back(LocaleMapping{
			"nl",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.dutch")});
		locales.emplace_back(LocaleMapping{
			"no",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.norwegian")});
		locales.emplace_back(LocaleMapping{
			"pl",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.polish")});
		locales.emplace_back(LocaleMapping{
			"pt",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.portuguese")});
		locales.emplace_back(LocaleMapping{
			"ro",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.romanian")});
		locales.emplace_back(LocaleMapping{
			"sk",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.slovak")});
		locales.emplace_back(LocaleMapping{
			"fi",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.finnish")});
		locales.emplace_back(LocaleMapping{
			"sv",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.swedish")});
		locales.emplace_back(LocaleMapping{
			"tl",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.tagalog")});
		locales.emplace_back(LocaleMapping{
			"vi",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.vietnamese")});
		locales.emplace_back(LocaleMapping{
			"tr",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.turkish")});
		locales.emplace_back(LocaleMapping{
			"cs",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.czech")});
		locales.emplace_back(LocaleMapping{
			"el",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.greek")});
		locales.emplace_back(LocaleMapping{
			"bg",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.bulgarian")});
		locales.emplace_back(LocaleMapping{
			"ru",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.russian")});
		locales.emplace_back(LocaleMapping{
			"uk",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.ukrainian")});
		locales.emplace_back(LocaleMapping{
			"ar",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.arabic")});
		locales.emplace_back(LocaleMapping{
			"ms",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.malay")});
		locales.emplace_back(LocaleMapping{
			"hi",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.hindi")});
		locales.emplace_back(LocaleMapping{
			"th",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.thai")});
		locales.emplace_back(LocaleMapping{
			"zh",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.chinese")});
		locales.emplace_back(LocaleMapping{
			"ja",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.japanese")});
		locales.emplace_back(LocaleMapping{
			"zh-hk",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.cantonese")});
		locales.emplace_back(LocaleMapping{
			"ko",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.korean")});
		locales.emplace_back(LocaleMapping{
			"asl",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.asl")});
		locales.emplace_back(LocaleMapping{
			"other",
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.language.other")});
		lastLocaleCode = curLocale;
	}

	return locales;
}

} // namespace

void LanguageSelection::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "languageSelection");
	_language.Load(data, "language");
	_useManualInput = obs_data_get_bool(data, "useManualInput");
}

void LanguageSelection::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	_language.Save(data, "language");
	obs_data_set_bool(data, "useManualInput", _useManualInput);
	obs_data_set_obj(obj, "languageSelection", data);
}

void LanguageSelection::SetStreamLanguage(const TwitchToken &token) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "broadcaster_language", _language.c_str());
	auto result = SendPatchRequest(token, "https://api.twitch.tv",
				       "/helix/channels",
				       {{"broadcaster_id", token.GetUserID()}},
				       data.Get());

	if (result.status != 204) {
		blog(LOG_INFO, "Failed to set stream language! (%d)",
		     result.status);
	}
}

static void populateLocaleSelection(QComboBox *list)
{
	for (const auto &[languageCode, name] : getLocales()) {
		list->addItem(name, languageCode);
	}
}

LanguageSelectionWidget::LanguageSelectionWidget(QWidget *parent)
	: QWidget(parent),
	  _languageText(new VariableLineEdit(this)),
	  _languageCombo(new QComboBox(this)),
	  _toggleInput(new QPushButton(this))
{
	populateLocaleSelection(_languageCombo);

	auto getCurrent = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.action.twitch.language.getCurrent"));
	connect(getCurrent, &QPushButton::clicked, this,
		&LanguageSelectionWidget::GetCurrentClicked);

	_toggleInput->setCheckable(true);
	_toggleInput->setMaximumWidth(11);
	SetButtonIcon(_toggleInput, GetThemeTypeName() == "Light"
					    ? ":/res/images/dots-vert.svg"
					    : "theme:Dark/dots-vert.svg");

	connect(_toggleInput, &QPushButton::toggled, this, [this]() {
		_languageText->setVisible(!_languageText->isVisible());
		_languageCombo->setVisible(!_languageCombo->isVisible());
	});

	connect(_languageText, &VariableLineEdit::editingFinished, this,
		&LanguageSelectionWidget::TextChanged);
	connect(_languageCombo, &QComboBox::currentIndexChanged, this,
		&LanguageSelectionWidget::ComboSelectionChanged);

	auto inputLayout = new QHBoxLayout;
	inputLayout->addWidget(_languageCombo);
	inputLayout->addWidget(_languageText);
	inputLayout->addWidget(_toggleInput);

	auto layout = new QVBoxLayout;
	layout->addLayout(inputLayout);
	layout->addWidget(getCurrent);
	setLayout(layout);
}

void LanguageSelectionWidget::SetLanguageSelection(
	const LanguageSelection &language)
{
	const QSignalBlocker b1(_languageText);
	const QSignalBlocker b2(_languageCombo);

	_languageText->setText(language._language);
	_languageCombo->setCurrentIndex(
		_languageCombo->findData(QString(language._language.c_str())));
	_toggleInput->setChecked(language._useManualInput);
	_languageText->setVisible(language._useManualInput);
	_languageCombo->setVisible(!language._useManualInput);
}

void LanguageSelectionWidget::TextChanged()
{
	LanguageSelection language;
	language._useManualInput = _toggleInput->isChecked();
	language._language = _languageText->text().toStdString();
	emit LanguageChanged(language);

	const QSignalBlocker b(this);
	SetLanguageSelection(language);
}

void LanguageSelectionWidget::ComboSelectionChanged()
{
	LanguageSelection language;
	language._useManualInput = _toggleInput->isChecked();
	language._language =
		_languageCombo->currentData().toString().toStdString();
	emit LanguageChanged(language);

	const QSignalBlocker b(this);
	SetLanguageSelection(language);
}

void LanguageSelectionWidget::SetToken(const std::weak_ptr<TwitchToken> &t)
{
	_token = t;
}

void LanguageSelectionWidget::GetCurrentClicked()
{
	auto token = _token.lock();
	if (!token) {
		return;
	}

	TwitchChannel channel;
	channel.SetName(token->Name());
	const auto channelInfo = channel.GetInfo(*token);
	if (!channelInfo) {
		return;
	}

	LanguageSelection language;
	language._language = channelInfo->broadcaster_language;

	SetLanguageSelection(language);
}

} // namespace advss
