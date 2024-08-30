#pragma once
#include "export-symbol-helper.hpp"

#include <obs-data.h>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QToolButton>
#include <QWidget>

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
	EXPORT void CreateBackwardsCompatibleRegex(bool enable,
						   bool setOptions = true);
	EXPORT QRegularExpression::PatternOptions GetPatternOptions() const;
	void SetPatternOptions(QRegularExpression::PatternOptions);
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

class ADVSS_EXPORT RegexConfigWidget : public QWidget {
	Q_OBJECT
public:
	RegexConfigWidget(QWidget *parent = nullptr, bool showEnable = true);
	void SetRegexConfig(const RegexConfig &);

public slots:
	void EnableChanged(bool);
	void OpenSettingsClicked();
signals:
	void RegexConfigChanged(const RegexConfig &);

private:
	void SetVisibility();

	QToolButton *_openSettings;
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
