#include "macro-search.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"
#include "macro-action-factory.hpp"
#include "macro-condition-factory.hpp"
#include "macro-helpers.hpp"
#include "macro-signals.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs.hpp>

#include <QComboBox>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>

namespace advss {

static bool searchEnabled = false;
static bool setup();
static bool setupDone = setup();

static QLayout *searchLayout = nullptr;
static QComboBox *searchType = nullptr;
static RegexConfigWidget *searchRegex = nullptr;
static QPushButton *showSettings = nullptr;
static std::function<void()> refreshFilter;

static void save(obs_data_t *data)
{
	GetMacroSearchSettings().Save(data, "macroSearchSettings");
}

static void load(obs_data_t *data)
{
	GetMacroSearchSettings().Load(data, "macroSearchSettings");
}

static bool setup()
{
	AddSaveStep(save);
	AddLoadStep(load);
	return true;
}

void MacroSearchSettings::Save(obs_data_t *data, const char *name)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "showAlways", showAlways);
	obs_data_set_int(settings, "searchType", static_cast<int>(searchType));
	obs_data_set_string(settings, "searchString", searchString.c_str());
	regex.Save(settings);
	obs_data_set_obj(data, name, settings);
}

void MacroSearchSettings::Load(obs_data_t *data, const char *name)
{
	OBSDataAutoRelease settings = obs_data_get_obj(data, name);
	showAlways = obs_data_get_bool(settings, "showAlways");
	searchType = static_cast<SearchType>(
		obs_data_get_int(settings, "searchType"));
	searchString = obs_data_get_string(settings, "searchString");
	regex.Load(settings);
}

static void showAdvancedSearchSettings(bool show)
{
	assert(searchType);
	assert(searchRegex);

	searchType->setVisible(show);
	searchRegex->setVisible(show);
}

static bool shouldShowSearch()
{
	static const int showSearchThreshold = 10;
	return GetMacroSearchSettings().showAlways ||
	       GetTopLevelMacros().size() >= showSearchThreshold;
}

void CheckMacroSearchVisibility()
{
	assert(searchLayout);
	assert(searchType);
	assert(searchRegex);
	assert(showSettings);

	searchEnabled = shouldShowSearch();
	SetLayoutVisible(searchLayout, searchEnabled);
	showAdvancedSearchSettings(false);
	showSettings->setChecked(false);
	refreshFilter();
}

void SetupMacroSearchWidgets(QLayout *searchLayoutArg, QLineEdit *searchText,
			     QPushButton *searchClear, QComboBox *searchTypeArg,
			     RegexConfigWidget *searchRegexArg,
			     QPushButton *showSettingsArg,
			     const std::function<void()> &refreshArg)
{
	searchLayout = searchLayoutArg;
	searchType = searchTypeArg;
	searchRegex = searchRegexArg;
	refreshFilter = refreshArg;
	showSettings = showSettingsArg;

	searchClear->setMaximumWidth(22);
	SetButtonIcon(searchClear, GetThemeTypeName() == "Light"
					   ? "theme:Light/close.svg"
					   : "theme:Dark/close.svg");
	searchClear->setDisabled(GetMacroSearchSettings().searchString.empty());

	QWidget::connect(searchClear, &QPushButton::clicked, searchClear,
			 [=]() {
				 searchText->setText("");
				 searchClear->setDisabled(true);
			 });

	const auto ph = searchText->placeholderText();
	const QFontMetrics fm(searchText->font());
	const int width = fm.horizontalAdvance(ph);
	// Add a little padding so the text isn't cramped
	searchText->setMinimumWidth(width + 10);

	searchText->setText(
		QString::fromStdString(GetMacroSearchSettings().searchString));
	QWidget::connect(searchText, &QLineEdit::textChanged, searchText,
			 [searchClear](const QString &text) {
				 GetMacroSearchSettings().searchString =
					 text.toStdString();
				 searchClear->setDisabled(text.isEmpty());
				 refreshFilter();
			 });

	searchType->addItem(
		obs_module_text("AdvSceneSwitcher.macroTab.search.name"),
		static_cast<int>(MacroSearchSettings::SearchType::NAME));
	searchType->addItem(
		obs_module_text("AdvSceneSwitcher.macroTab.search.allSegments"),
		static_cast<int>(
			MacroSearchSettings::SearchType::ALL_SEGMENTS));
	searchType->addItem(
		obs_module_text("AdvSceneSwitcher.macroTab.search.conditions"),
		static_cast<int>(MacroSearchSettings::SearchType::CONDITIONS));
	searchType->addItem(
		obs_module_text("AdvSceneSwitcher.macroTab.search.actions"),
		static_cast<int>(MacroSearchSettings::SearchType::ACTIONS));
	searchType->addItem(
		obs_module_text("AdvSceneSwitcher.macroTab.search.label"),
		static_cast<int>(MacroSearchSettings::SearchType::LABEL));
	searchType->setCurrentIndex(searchType->findData(
		static_cast<int>(GetMacroSearchSettings().searchType)));
	QWidget::connect(
		searchType, &QComboBox::currentIndexChanged, searchType, []() {
			GetMacroSearchSettings().searchType =
				static_cast<MacroSearchSettings::SearchType>(
					searchType->currentData().toInt());
			refreshFilter();
		});

	searchRegex->SetRegexConfig(GetMacroSearchSettings().regex);
	QWidget::connect(searchRegex, &RegexConfigWidget::RegexConfigChanged,
			 searchRegex, [](const RegexConfig &regex) {
				 GetMacroSearchSettings().regex = regex;
				 refreshFilter();
			 });

	showSettings->setCheckable(true);
	showSettings->setMaximumWidth(11);
	SetButtonIcon(showSettings, GetThemeTypeName() == "Light"
					    ? ":/res/images/dots-vert.svg"
					    : "theme:Dark/dots-vert.svg");
	QWidget::connect(showSettings, &QPushButton::toggled, showSettings,
			 showAdvancedSearchSettings);

	QWidget::connect(MacroSignalManager::Instance(),
			 &MacroSignalManager::Rename, searchText,
			 []() { refreshFilter(); });

	CheckMacroSearchVisibility();
}

MacroSearchSettings &GetMacroSearchSettings()
{
	static MacroSearchSettings settings;
	return settings;
}

static bool stringMatches(const std::string &text)
{
	const auto &settings = GetMacroSearchSettings();
	const auto &regex = settings.regex;
	const auto &searchString = settings.searchString;

	if (regex.Enabled()) {
		return regex.Matches(text, searchString);
	}

	return QString::fromStdString(text).contains(
		QString::fromStdString(searchString), Qt::CaseInsensitive);
}

static bool segmentTypeMatches(MacroSegment *segment, bool isCondition)
{
	if (!segment) {
		return false;
	}

	const auto getNameFromId =
		isCondition ? MacroConditionFactory::GetConditionName
			    : MacroActionFactory::GetActionName;
	const auto name = getNameFromId(segment->GetId());
	return stringMatches(obs_module_text(name.c_str()));
}

static bool segmentLabelMatches(MacroSegment *segment)
{
	if (!segment) {
		return false;
	}

	if (!segment->GetUseCustomLabel()) {
		return false;
	}

	const auto label = segment->GetCustomLabel();
	return stringMatches(label);
}

bool MacroMatchesSearchFilter(Macro *macro)
{
	assert(macro);

	if (!searchEnabled) {
		return true;
	}

	const auto &settings = GetMacroSearchSettings();

	if (settings.searchString.empty()) {
		return true;
	}

	if (macro->IsGroup()) {
		const auto groupEntries = GetGroupMacroEntries(macro);
		for (const auto &entry : groupEntries) {
			if (MacroMatchesSearchFilter(entry.get())) {
				return true;
			}
		}
		return false;
	}

	switch (settings.searchType) {
	case MacroSearchSettings::SearchType::NAME: {
		const auto name = macro->Name();
		return stringMatches(name);
	}
	case MacroSearchSettings::SearchType::ALL_SEGMENTS:
		for (const auto &condition : macro->Conditions()) {
			if (segmentTypeMatches(condition.get(), true)) {
				return true;
			}
		}
		for (const auto &action : macro->Actions()) {
			if (segmentTypeMatches(action.get(), false)) {
				return true;
			}
		}
		for (const auto &action : macro->ElseActions()) {
			if (segmentTypeMatches(action.get(), false)) {
				return true;
			}
		}
		return false;
	case MacroSearchSettings::SearchType::CONDITIONS:
		for (const auto &condition : macro->Conditions()) {
			if (segmentTypeMatches(condition.get(), true)) {
				return true;
			}
		}
		return false;
	case MacroSearchSettings::SearchType::ACTIONS:
		for (const auto &action : macro->Actions()) {
			if (segmentTypeMatches(action.get(), false)) {
				return true;
			}
		}
		for (const auto &action : macro->ElseActions()) {
			if (segmentTypeMatches(action.get(), false)) {
				return true;
			}
		}
		return false;
	case MacroSearchSettings::SearchType::LABEL:
		for (const auto &condition : macro->Conditions()) {
			if (segmentLabelMatches(condition.get())) {
				return true;
			}
		}
		for (const auto &action : macro->Actions()) {
			if (segmentLabelMatches(action.get())) {
				return true;
			}
		}
		for (const auto &action : macro->ElseActions()) {
			if (segmentLabelMatches(action.get())) {
				return true;
			}
		}
		return false;
	default:
		break;
	}

	// Unhandled search type
	assert(false);
	return true;
}

} // namespace advss
