#pragma once
#include "obs-data.h"

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QRegularExpression>

namespace advss {

class RegexConfigWidget;
class RegexConfigDialog;

class RegexConfig {
public:
	EXPORT RegexConfig(bool enabled = false);

	EXPORT void Save(obs_data_t *obj,
			 const char *name = "regexConfig") const;
	EXPORT void Load(obs_data_t *obj, const char *name = "regexConfig");

	EXPORT bool Enabled() const { return _enable; }
	EXPORT void SetEnabled(bool enable) { _enable = enable; }
	EXPORT void CreateBackwardsCompatibleRegex(bool, bool = true);
	EXPORT QRegularExpression::PatternOptions GetPatternOptions() const
	{
		return _options;
	};
	EXPORT QRegularExpression GetRegularExpression(const QString &) const;
	EXPORT QRegularExpression
	GetRegularExpression(const std::string &) const;
	EXPORT bool Matches(const QString &text,
			    const QString &expression) const;
	EXPORT bool Matches(const std::string &text,
			    const std::string &expression) const;

	EXPORT static RegexConfig PartialMatchRegexConfig();

private:
	bool _enable = false;
	bool _partialMatch = false;
	QRegularExpression::PatternOptions _options =
		QRegularExpression::NoPatternOption;
	friend RegexConfigWidget;
	friend RegexConfigDialog;
};

class RegexConfigWidget : public QWidget {
	Q_OBJECT
public:
	EXPORT RegexConfigWidget(QWidget *parent = nullptr,
				 bool showEnable = true);
	EXPORT void SetRegexConfig(const RegexConfig &);

public slots:
	EXPORT void EnableChanged(bool);
	EXPORT void OpenSettingsClicked();
signals:
	void RegexConfigChanged(const RegexConfig &);

private:
	void SetVisibility();

	QPushButton *_openSettings;
	QPushButton *_enable;
	RegexConfig _config;
};

class RegexConfigDialog : public QDialog {
	Q_OBJECT

public:
	RegexConfigDialog(QWidget *parent, const RegexConfig &);
	static bool AskForSettings(QWidget *parent, RegexConfig &settings);

private:
	static void SetOption(RegexConfig &, QRegularExpression::PatternOptions,
			      QCheckBox *);

	QCheckBox *_partialMatch;
	QCheckBox *_caseInsensitive;
	QCheckBox *_dotMatch;
	QCheckBox *_multiLine;
	QCheckBox *_extendedPattern;
	QDialogButtonBox *_buttonbox;
};

} // namespace advss
