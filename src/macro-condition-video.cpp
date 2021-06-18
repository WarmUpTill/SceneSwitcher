#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-video.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <QFileDialog>
#include <QBuffer>
#include <QToolTip>
#include <QMessageBox>

const std::string MacroConditionVideo::id = "video";

bool MacroConditionVideo::_registered = MacroConditionFactory::Register(
	MacroConditionVideo::id,
	{MacroConditionVideo::Create, MacroConditionVideoEdit::Create,
	 "AdvSceneSwitcher.condition.video"});

static std::map<VideoCondition, std::string> conditionTypes = {
	{VideoCondition::MATCH,
	 "AdvSceneSwitcher.condition.video.condition.match"},
	{VideoCondition::DIFFER,
	 "AdvSceneSwitcher.condition.video.condition.differ"},
	{VideoCondition::HAS_NOT_CHANGED,
	 "AdvSceneSwitcher.condition.video.condition.hasNotChanged"},
	{VideoCondition::HAS_CHANGED,
	 "AdvSceneSwitcher.condition.video.condition.hasChanged"},
	{VideoCondition::NO_IMAGE,
	 "AdvSceneSwitcher.condition.video.condition.noImage"},
};

bool requiresFileInput(VideoCondition t)
{
	return t == VideoCondition::MATCH || t == VideoCondition::DIFFER;
}

bool MacroConditionVideo::CheckCondition()
{
	bool match = false;

	if (_screenshotData) {
		if (_screenshotData->done) {
			match = Compare();

			if (!requiresFileInput(_condition)) {
				_matchImage = std::move(_screenshotData->image);
			}
			_screenshotData.reset(nullptr);
		}
	}

	GetScreenshot();
	return match;
}

bool MacroConditionVideo::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "videoSource",
			    GetWeakSourceName(_videoSource).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "filePath", _file.c_str());
	return true;
}

bool MacroConditionVideo::Load(obs_data_t *obj)
{
	const char *videoSourceName = obs_data_get_string(obj, "videoSource");
	_videoSource = GetWeakSourceByName(videoSourceName);
	_condition =
		static_cast<VideoCondition>(obs_data_get_int(obj, "condition"));
	_file = obs_data_get_string(obj, "filePath");

	if (requiresFileInput(_condition)) {
		(void)LoadImageFromFile();
	}
	return true;
}

std::string MacroConditionVideo::GetShortDesc()
{
	if (_videoSource) {
		return GetWeakSourceName(_videoSource);
	}
	return "";
}

void MacroConditionVideo::GetScreenshot()
{
	auto source = obs_weak_source_get_source(_videoSource);
	_screenshotData = std::make_unique<AdvSSScreenshotObj>(source);
	obs_source_release(source);
}

bool MacroConditionVideo::LoadImageFromFile()
{
	if (!_matchImage.load(QString::fromStdString(_file))) {
		blog(LOG_WARNING, "Cannot load image data from file '%s'",
		     _file.c_str());
		return false;
	}
	_matchImage =
		_matchImage.convertToFormat(QImage::Format::Format_RGBX8888);
	return true;
}

bool MacroConditionVideo::Compare()
{
	switch (_condition) {
	case VideoCondition::MATCH:
		return _screenshotData->image == _matchImage;
	case VideoCondition::DIFFER:
		return _screenshotData->image != _matchImage;
	case VideoCondition::HAS_CHANGED:
		return _screenshotData->image != _matchImage;
	case VideoCondition::HAS_NOT_CHANGED:
		return _screenshotData->image == _matchImage;
	case VideoCondition::NO_IMAGE:
		return _screenshotData->image.isNull();
	default:
		break;
	}
	return false;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionVideoEdit::MacroConditionVideoEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVideo> entryData)
	: QWidget(parent)
{
	_videoSelection = new QComboBox();
	_condition = new QComboBox();
	_filePath = new QLineEdit();
	_browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));

	_filePath->setFixedWidth(100);

	_browseButton->setStyleSheet("border:1px solid gray;");

	QWidget::connect(_videoSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(_browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));

	populateVideoSelection(_videoSelection);
	populateConditionSelection(_condition);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoSources}}", _videoSelection},
		{"{{condition}}", _condition},
		{"{{filePath}}", _filePath},
		{"{{browseButton}}", _browseButton},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.video.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionVideoEdit::UpdatePreviewTooltip()
{
	if (!_entryData) {
		return;
	}

	if (!requiresFileInput(_entryData->_condition)) {
		this->setToolTip("");
		return;
	}

	QImage preview = _entryData->GetMatchImage().scaled(
		{300, 300}, Qt::KeepAspectRatio);

	QByteArray data;
	QBuffer buffer(&data);
	if (!preview.save(&buffer, "PNG")) {
		return;
	}

	QString html =
		QString("<html><img src='data:image/png;base64, %0'/></html>")
			.arg(QString(data.toBase64()));
	this->setToolTip(html);
}

void MacroConditionVideoEdit::SetFilePath(const QString &text)
{
	_filePath->setText(text);
	FilePathChanged();
}

void MacroConditionVideoEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_videoSource = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionVideoEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<VideoCondition>(cond);

	if (requiresFileInput(_entryData->_condition)) {
		_filePath->show();
		_browseButton->show();
	} else {
		_filePath->hide();
		_browseButton->hide();
	}

	// Reload image data to avoid incorrect matches.
	//
	// Condition type HAS_NOT_CHANGED will use matchImage to store previous
	// frame of video source, which will differ from the image stored at
	// specified file location.
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void MacroConditionVideoEdit::FilePathChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_file = _filePath->text().toUtf8().constData();
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void MacroConditionVideoEdit::BrowseButtonClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	QString path;
	bool useExistingFile = false;
	// Ask whether to create screenshot or to select existing file
	if (_entryData->_videoSource) {
		QMessageBox msgBox(
			QMessageBox::Question,
			obs_module_text("AdvSceneSwitcher.windowTitle"),
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction"),
			QMessageBox::Yes | QMessageBox::No);
		msgBox.setWindowFlags(Qt::Window | Qt::WindowTitleHint |
				      Qt::CustomizeWindowHint);
		msgBox.setButtonText(
			QMessageBox::Yes,
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction.file"));
		msgBox.setButtonText(
			QMessageBox::No,
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction.screenshot"));
		useExistingFile = msgBox.exec() == QMessageBox::Yes;
	}

	if (useExistingFile) {
		path = QFileDialog::getOpenFileName(this);
		if (path.isEmpty()) {
			return;
		}

	} else {
		auto source =
			obs_weak_source_get_source(_entryData->_videoSource);
		auto screenshot = std::make_unique<AdvSSScreenshotObj>(source);
		obs_source_release(source);

		path = QFileDialog::getSaveFileName(this);
		if (path.isEmpty()) {
			return;
		}
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return;
		}
		if (!screenshot->done) { // Screenshot usually completed by now
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		if (!screenshot->done) {
			DisplayMessage("Failed to get screenshot of source!");
			return;
		}

		screenshot->image.save(path);
	}
	SetFilePath(path);
}

void MacroConditionVideoEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_videoSelection->setCurrentText(
		GetWeakSourceName(_entryData->_videoSource).c_str());
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_filePath->setText(QString::fromStdString(_entryData->_file));

	if (!requiresFileInput(_entryData->_condition)) {
		_filePath->hide();
		_browseButton->hide();
	}
}
