#include "regex-config.hpp"
#include "utility.hpp"

#include <QLayout>
#include <obs-module.h>

void RegexConfig::Save(obs_data_t *obj)
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "enable", _enable);
	obs_data_set_bool(data, "partial", _partialMatch);
	obs_data_set_int(data, "options", _options);
	obs_data_set_obj(obj, "regexConfig", data);
	obs_data_release(data);
}

void RegexConfig::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "regexConfig");
	_enable = obs_data_get_bool(data, "enable");
	_partialMatch = obs_data_get_bool(data, "partial");
	_options = static_cast<QRegularExpression::PatternOption>(
		obs_data_get_int(data, "options"));
	obs_data_release(data);
}

void RegexConfig::CreateBackwardsCompatibleRegex(bool enable, bool setOptions)
{
	_enable = enable;
	if (setOptions) {
		_options |= QRegularExpression::DotMatchesEverythingOption;
	}
}

QRegularExpression RegexConfig::GetRegularExpression(const QString &expr) const
{
	if (_partialMatch) {
		return QRegularExpression(expr, _options);
	}
	return QRegularExpression(QRegularExpression::anchoredPattern(expr),
				  _options);
}

QRegularExpression
RegexConfig::GetRegularExpression(const std::string &expr) const
{
	return GetRegularExpression(QString::fromStdString(expr));
}

RegexConfigWidget::RegexConfigWidget(QWidget *parent)
	: QWidget(parent),
	  _openSettings(new QPushButton()),
	  _enable(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.regex.enable")))
{
	_openSettings->setMaximumWidth(22);
	setButtonIcon(_openSettings, ":/settings/images/settings/general.svg");
	_openSettings->setFlat(true);

	QWidget::connect(_enable, SIGNAL(stateChanged(int)), this,
			 SLOT(EnableChanged(int)));
	QWidget::connect(_openSettings, SIGNAL(clicked()), this,
			 SLOT(OpenSettingsClicked()));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_enable);
	layout->addWidget(_openSettings);
	setLayout(layout);
}

void RegexConfigWidget::SetRegexConfig(const RegexConfig &config)
{
	_config = config;
	_enable->setChecked(config._enable);
	SetVisibility();
}

void RegexConfigWidget::OpenSettingsClicked()
{
	bool accepted = RegexConfigDialog::AskForSettings(this, _config);
	if (!accepted) {
		return;
	}
	emit RegexConfigChanged(_config);
}

void RegexConfigWidget::SetVisibility()
{
	_openSettings->setVisible(_config._enable);
	adjustSize();
	updateGeometry();
}

void RegexConfigWidget::EnableChanged(int value)
{
	_config._enable = value;
	SetVisibility();
	emit RegexConfigChanged(_config);
}

RegexConfigDialog::RegexConfigDialog(QWidget *parent, const RegexConfig &config)
	: QDialog(parent),
	  _partialMatch(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.regex.partialMatch"))),
	  _caseInsensitive(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.regex.caseInsensitive"))),
	  _dotMatch(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.regex.dotMatchNewline"))),
	  _multiLine(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.regex.multiLine"))),
	  _extendedPattern(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.regex.extendedPattern"))),
	  _buttonbox(new QDialogButtonBox(QDialogButtonBox::Ok |
					  QDialogButtonBox::Cancel))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	_partialMatch->setChecked(config._partialMatch);
	_caseInsensitive->setChecked(config._options &
				     QRegularExpression::CaseInsensitiveOption);
	_dotMatch->setChecked(config._options &
			      QRegularExpression::DotMatchesEverythingOption);
	_multiLine->setChecked(config._options &
			       QRegularExpression::MultilineOption);
	_extendedPattern->setChecked(
		config._options &
		QRegularExpression::ExtendedPatternSyntaxOption);

	QWidget::connect(_buttonbox, &QDialogButtonBox::accepted, this,
			 &QDialog::accept);
	QWidget::connect(_buttonbox, &QDialogButtonBox::rejected, this,
			 &QDialog::reject);

	auto layout = new QVBoxLayout();
	layout->addWidget(_partialMatch);
	layout->addWidget(_caseInsensitive);
	layout->addWidget(_dotMatch);
	layout->addWidget(_multiLine);
	layout->addWidget(_extendedPattern);
	layout->addWidget(_buttonbox, Qt::AlignHCenter);
	setLayout(layout);
}

void RegexConfigDialog::SetOption(RegexConfig &conf,
				  QRegularExpression::PatternOptions what,
				  QCheckBox *cb)
{
	if (cb->isChecked()) {
		conf._options |= what;
	} else {
		conf._options &= ~what;
	}
}

bool RegexConfigDialog::AskForSettings(QWidget *parent, RegexConfig &settings)
{
	RegexConfigDialog dialog(parent, settings);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	settings._partialMatch = dialog._partialMatch->isChecked();
	SetOption(settings, QRegularExpression::CaseInsensitiveOption,
		  dialog._caseInsensitive);
	SetOption(settings, QRegularExpression::DotMatchesEverythingOption,
		  dialog._dotMatch);
	SetOption(settings, QRegularExpression::MultilineOption,
		  dialog._multiLine);
	SetOption(settings, QRegularExpression::ExtendedPatternSyntaxOption,
		  dialog._extendedPattern);
	return true;
}
