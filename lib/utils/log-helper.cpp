#include "log-helper.hpp"
#include "obs-module-helper.hpp"

#include <map>
#include <obs-data.h>
#include <QComboBox>
#include <string>

namespace advss {

static LogLevel logLevel = LogLevel::DEFAULT;

bool VerboseLoggingEnabled()
{
	return logLevel == LogLevel::VERBOSE;
}

bool ActionLoggingEnabled()
{
	return logLevel == LogLevel::LOG_ACTION || VerboseLoggingEnabled();
}

bool MacroLoggingEnabled()
{
	return logLevel == LogLevel::LOG_MACRO || ActionLoggingEnabled();
}

bool LoggingEnabled()
{
	return logLevel != LogLevel::DISABLE;
}

void SetLogLevel(LogLevel newLogLevel)
{
	logLevel = newLogLevel;
}

LogLevel GetLogLevel()
{
	return logLevel;
}

void SaveLogLevel(void *data)
{
	auto obj = static_cast<obs_data_t *>(data);
	obs_data_set_int(obj, "logLevel", static_cast<int>(logLevel));
	obs_data_set_int(obj, "logLevelVersion", 1);
}

void LoadLogLevel(void *data)
{
	auto obj = static_cast<obs_data_t *>(data);
	logLevel = static_cast<LogLevel>(obs_data_get_int(obj, "logLevel"));
	if (obs_data_get_int(obj, "logLevelVersion") < 1) {
		enum OldLogLevel { DEFAULT, LOG_ACTION, VERBOSE };
		OldLogLevel oldLogLevel = static_cast<OldLogLevel>(
			obs_data_get_int(obj, "logLevel"));
		switch (oldLogLevel) {
		case DEFAULT:
			logLevel = LogLevel::DEFAULT;
			break;
		case LOG_ACTION:
			logLevel = LogLevel::LOG_ACTION;
			break;
		case VERBOSE:
			logLevel = LogLevel::VERBOSE;
			break;
		default:
			break;
		}
	}
}

void PopulateLogLevelSelection(void *sel)
{
	auto selection = static_cast<QComboBox *>(sel);
	static const std::map<LogLevel, std::string> logLevels = {
		{LogLevel::DISABLE,
		 "AdvSceneSwitcher.generalTab.generalBehavior.logLevel.disable"},
		{LogLevel::DEFAULT,
		 "AdvSceneSwitcher.generalTab.generalBehavior.logLevel.default"},
		{LogLevel::LOG_MACRO,
		 "AdvSceneSwitcher.generalTab.generalBehavior.logLevel.logMacro"},
		{LogLevel::LOG_ACTION,
		 "AdvSceneSwitcher.generalTab.generalBehavior.logLevel.logAction"},
		{LogLevel::VERBOSE,
		 "AdvSceneSwitcher.generalTab.generalBehavior.logLevel.verbose"},
	};

	selection->clear();
	for (const auto &[value, name] : logLevels) {
		selection->addItem(obs_module_text(name.c_str()),
				   static_cast<int>(value));
	}
}

} // namespace advss
