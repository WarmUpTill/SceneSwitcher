#include "macro-condition-video.hpp"

#include <layout-helpers.hpp>
#include <plugin-state-helpers.hpp>
#include <ui-helpers.hpp>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QVBoxLayout>

namespace advss {

const static std::map<tesseract::PageSegMode, std::string> pageSegModes = {
	{tesseract::PageSegMode::PSM_SINGLE_COLUMN,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleColumn"},
	{tesseract::PageSegMode::PSM_SINGLE_BLOCK_VERT_TEXT,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleBlockVertText"},
	{tesseract::PageSegMode::PSM_SINGLE_BLOCK,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleBlock"},
	{tesseract::PageSegMode::PSM_SINGLE_LINE,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleLine"},
	{tesseract::PageSegMode::PSM_SINGLE_WORD,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleWord"},
	{tesseract::PageSegMode::PSM_CIRCLE_WORD,
	 "AdvSceneSwitcher.condition.video.ocrMode.circleWord"},
	{tesseract::PageSegMode::PSM_SINGLE_CHAR,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleChar"},
	{tesseract::PageSegMode::PSM_SPARSE_TEXT,
	 "AdvSceneSwitcher.condition.video.ocrMode.sparseText"},
	{tesseract::PageSegMode::PSM_SPARSE_TEXT_OSD,
	 "AdvSceneSwitcher.condition.video.ocrMode.sparseTextOSD"},
};

static inline void populatePageSegModeSelection(QComboBox *list)
{
	for (const auto &[mode, name] : pageSegModes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(mode));
	}
}

static void openFileInEditor(const std::string &filepath)
{
	const auto path = QString::fromStdString(filepath);
	const QFileInfo fileInfo(path);
	if (!fileInfo.exists()) {
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			DisplayMessage(obs_module_text(
				"AdvSceneSwitcher.condition.video.ocrOpenConfig.createFailed"));
			return;
		}
		file.close();
	}

	QUrl fileUrl = QUrl::fromLocalFile(path);
	if (!QDesktopServices::openUrl(fileUrl)) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.ocrOpenConfig.openFailed"));
	}
}

OCREdit::OCREdit(QWidget *parent, PreviewDialog *previewDialog,
		 const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _matchText(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(this)),
	  _colorButton(new VariableColorButton(
		  this,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.selectColor"))),
	  _colorThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThresholdDescription"),
		  true)),
	  _pageSegMode(new QComboBox()),
	  _tesseractBaseDir(new FileSelection(FileSelection::Type::FOLDER)),
	  _languageCode(new VariableLineEdit(this)),
	  _useConfig(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.ocrUseConfigFile"))),
	  _configFile(new FileSelection(FileSelection::Type::WRITE, this)),
	  _openConfigFile(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.ocrOpenConfigFile"))),
	  _reloadConfig(new QPushButton()),
	  _configLayout(new QHBoxLayout()),
	  _previewDialog(previewDialog),
	  _entryData(data)
{
	populatePageSegModeSelection(_pageSegMode);

	_reloadConfig->setMaximumWidth(22);
	SetButtonIcon(_reloadConfig, GetThemeTypeName() == "Light"
					     ? ":res/images/refresh.svg"
					     : "theme:Dark/refresh.svg");
	_reloadConfig->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.ocrConfigReload"));

	QWidget::connect(_colorButton,
			 SIGNAL(ColorVariableChanged(const ColorVariable &)),
			 this, SLOT(ColorChanged(const ColorVariable &)));
	QWidget::connect(
		_colorThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ColorThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_pageSegMode, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(PageSegModeChanged(int)));
	QWidget::connect(_tesseractBaseDir,
			 SIGNAL(PathChanged(const QString &)), this,
			 SLOT(TesseractBaseDirChanged(const QString &)));
	QWidget::connect(_languageCode, SIGNAL(editingFinished()), this,
			 SLOT(LanguageChanged()));
	QWidget::connect(_useConfig, SIGNAL(stateChanged(int)), this,
			 SLOT(UseConfigChanged(int)));
	QWidget::connect(_configFile, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ConfigFileChanged(const QString &)));
	QWidget::connect(_openConfigFile, &QPushButton::clicked, [this](bool) {
		openFileInEditor(
			_entryData->_ocrParameters.GetCustomConfigFile());
	});
	QWidget::connect(_reloadConfig, &QPushButton::clicked, [this](bool) {
		GUARD_LOADING_AND_LOCK();
		_entryData->_ocrParameters.EnableCustomConfig(true);
		_previewDialog->OCRParametersChanged(
			_entryData->_ocrParameters);
	});

	auto configFileHint = new QLabel();
	const QString path = GetThemeTypeName() == "Light"
				     ? ":/res/images/help.svg"
				     : ":/res/images/help_light.svg";
	const QIcon icon(path);
	const QPixmap pixmap = icon.pixmap(QSize(16, 16));
	configFileHint->setPixmap(pixmap);
	configFileHint->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.ocrConfigHint"));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{color}}", _colorButton},
		{"{{textType}}", _pageSegMode},
		{"{{tesseractBaseDir}}", _tesseractBaseDir},
		{"{{languageCode}}", _languageCode},
		{"{{configFile}}", _configFile},
		{"{{openConfigFile}}", _openConfigFile},
		{"{{reloadConfig}}", _reloadConfig},
		{"{{configFileHint}}", configFileHint},
	};

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	auto textLayout = new QHBoxLayout();
	textLayout->setContentsMargins(0, 0, 0, 0);
	textLayout->addWidget(_matchText);
	textLayout->addWidget(_regex);
	layout->addLayout(textLayout);
	auto pageModeSegLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.ocrTextType"),
		pageModeSegLayout, widgetPlaceholders);
	layout->addLayout(pageModeSegLayout);
	auto baseDirLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.ocrBaseDir"),
		baseDirLayout, widgetPlaceholders, false);
	layout->addLayout(baseDirLayout);
	auto languageLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.ocrLanguage"),
		languageLayout, widgetPlaceholders);
	layout->addLayout(languageLayout);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.ocrConfig"),
		_configLayout, widgetPlaceholders, false);
	layout->addWidget(_useConfig);
	layout->addLayout(_configLayout);
	auto colorPickLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.ocrColorPick"),
		colorPickLayout, widgetPlaceholders);
	layout->addLayout(colorPickLayout);
	layout->addWidget(_colorThreshold);
	setLayout(layout);

	_matchText->setPlainText(_entryData->_ocrParameters.text);
	_regex->SetRegexConfig(_entryData->_ocrParameters.regex);
	_colorButton->SetValue(_entryData->_ocrParameters.color);
	_colorThreshold->SetDoubleValue(
		_entryData->_ocrParameters.colorThreshold);
	_pageSegMode->setCurrentIndex(_pageSegMode->findData(
		static_cast<int>(_entryData->_ocrParameters.GetPageMode())));
	_tesseractBaseDir->SetPath(
		_entryData->_ocrParameters.GetTesseractBasePath());
	_languageCode->setText(_entryData->_ocrParameters.GetLanguageCode());
	_useConfig->setChecked(
		_entryData->_ocrParameters.CustomConfigIsEnabled());
	_configFile->SetPath(_entryData->_ocrParameters.GetCustomConfigFile());
	SetLayoutVisible(_configLayout,
			 _entryData->_ocrParameters.CustomConfigIsEnabled());
	_loading = false;
}

void OCREdit::ColorChanged(const ColorVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_ocrParameters.color = value;

	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::ColorThresholdChanged(const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_ocrParameters.colorThreshold = value;

	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::MatchTextChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_ocrParameters.text =
		_matchText->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();

	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::RegexChanged(const RegexConfig &conf)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_ocrParameters.regex = conf;
	adjustSize();
	updateGeometry();

	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::PageSegModeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetPageSegMode(static_cast<tesseract::PageSegMode>(
		_pageSegMode->itemData(idx).toInt()));

	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::TesseractBaseDirChanged(const QString &path)
{
	GUARD_LOADING_AND_LOCK();
	if (!_entryData->SetTesseractBaseDir(path.toStdString())) {
		const QString message(obs_module_text(
			"AdvSceneSwitcher.condition.video.ocrLanguageNotFound"));
		const QDir dataDir(path);
		const QString fileName(_languageCode->text() + ".traineddata");
		DisplayMessage(message.arg(fileName, dataDir.absolutePath()));

		// Reset to previous value
		const QSignalBlocker b(this);
		_tesseractBaseDir->SetPath(
			_entryData->_ocrParameters.GetTesseractBasePath());
		return;
	}
	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::LanguageChanged()
{
	GUARD_LOADING_AND_LOCK();
	if (!_entryData->SetLanguageCode(_languageCode->text().toStdString())) {
		const QString message(obs_module_text(
			"AdvSceneSwitcher.condition.video.ocrLanguageNotFound"));
		const QDir dataDir(QString::fromStdString(
			_entryData->_ocrParameters.GetTesseractBasePath()));
		const QString fileName(_languageCode->text() + ".traineddata");
		DisplayMessage(message.arg(fileName, dataDir.absolutePath()));

		// Reset to previous value
		const QSignalBlocker b(this);
		_languageCode->setText(
			_entryData->_ocrParameters.GetLanguageCode());
		return;
	}
	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::UseConfigChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_ocrParameters.EnableCustomConfig(value);
	SetLayoutVisible(_configLayout, value);
	adjustSize();
	updateGeometry();
	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

void OCREdit::ConfigFileChanged(const QString &path)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_ocrParameters.SetCustomConfigFile(path.toStdString());
	_previewDialog->OCRParametersChanged(_entryData->_ocrParameters);
}

} // namespace advss
