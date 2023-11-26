#include "category-selection.hpp"
#include "token.hpp"
#include "twitch-helpers.hpp"

#include <utility.hpp>
#include <name-dialog.hpp>
#include <obs-module-helper.hpp>
#include <obs.hpp>
#include <QVBoxLayout>

namespace advss {

void TwitchCategory::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "category");
	id = obs_data_get_int(data, "id");
	name = obs_data_get_string(data, "name");
}

void TwitchCategory::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "id", id);
	obs_data_set_string(data, "name", name.c_str());
	obs_data_set_obj(obj, "category", data);
}

bool TwitchCategorySelection::_fetchingCategoriesDone = false;
std::map<QString, int> TwitchCategorySelection::_streamingCategories;

void TwitchCategorySelection::PopulateCategorySelection()
{
	auto token = _token.lock();
	if (!token && _streamingCategories.empty()) {
		return;
	}

	if (!_fetchingCategoriesDone && token) {
		_categoryGrabber.Start(token);
		if (_progressDialog->exec() == QDialog::Accepted) {
			_fetchingCategoriesDone = true;
		}
		_categoryGrabber.Stop();
		_categoryGrabber.wait();
	}
	UpdateCategoryList();
}

void TwitchCategorySelection::UpdateCategoryList()
{
	_streamingCategories = _categoryGrabber.GetCategories();
	QString currentSelection = currentText();

	const QSignalBlocker b(this);
	clear();
	for (const auto &[name, id] : _streamingCategories) {
		addItem(name, id);
	}

	setCurrentText(currentSelection);
}

TwitchCategorySelection::TwitchCategorySelection(QWidget *parent)
	: FilterComboBox(
		  parent,
		  obs_module_text("AdvSceneSwitcher.twitchCategories.select")),
	  _progressDialog(new ProgressDialog(this))
{
	_progressDialog->setWindowModality(Qt::WindowModal);
	setSizeAdjustPolicy(QComboBox::AdjustToContents);

	QWidget::connect(this, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));
	QWidget::connect(&_categoryGrabber, SIGNAL(CategoryCountUpdated(int)),
			 _progressDialog, SLOT(CategoryCountUpdated(int)));
	QWidget::connect(&_categoryGrabber, SIGNAL(Finished()), this,
			 SLOT(PopulateFinished()));
	QWidget::connect(TwitchCategorySignalManager::Instance(),
			 SIGNAL(RepopulateRequired()), this,
			 SLOT(UpdateCategoryList()));
}

void TwitchCategorySelection::SetToken(const std::weak_ptr<TwitchToken> &token)
{
	_token = token;
	const bool expired = token.expired();
	setDisabled(expired);
	if (expired) {
		setToolTip(obs_module_text(
			"AdvSceneSwitcher.action.twitch.categorySelectionDisabled"));
	} else {
		setToolTip("");
	}
}

void TwitchCategorySelection::PopulateFinished()
{
	_progressDialog->accept();
}

void TwitchCategorySelection::showPopup()
{
	if (!IsPopulated()) {
		PopulateCategorySelection();
	}
	adjustSize();
	updateGeometry();
	FilterComboBox::showPopup();
}

bool TwitchCategorySelection::IsPopulated()
{
	return count() == (int)_categoryGrabber.GetCategories().size() &&
	       _fetchingCategoriesDone;
}

void TwitchCategorySelection::SetCategory(const TwitchCategory &id)
{
	// If the list is populated already try to find id ...
	int index = findData(id.id);
	if (index != -1) {
		setCurrentIndex(index);
		return;
	}

	if (id.id == -1) {
		setCurrentIndex(-1);
		return;
	}

	// ... otherwise just add a dummy entry with the category name
	addItem(QString::fromStdString(id.name), id.id);
	setCurrentIndex(findData(id.id));
}

void TwitchCategorySelection::SelectionChanged(int index)
{
	TwitchCategory category{itemData(index).toInt(),
				currentText().toStdString()};
	emit CategoreyChanged(category);
}

std::map<QString, int> CategoryGrabber::_categoryMap = {};
std::mutex CategoryGrabber::_mtx = {};

void CategoryGrabber::Start(const std::shared_ptr<TwitchToken> &token,
			    const std::string search)
{
	_searchString = search;
	_token = token;
	_stop = false;
	start();
}

void CategoryGrabber::Stop()
{
	_stop = true;
}

const std::map<QString, int> &CategoryGrabber::GetCategories()
{
	return _categoryMap;
}

void CategoryGrabber::run()
{
	if (!_token) {
		return;
		emit Failed();
	}

	{
		std::lock_guard<std::mutex> lock(_mtx);
		if (_searchString.empty()) {
			GetAll();
		} else {
			Search(_searchString);
		}
	}

	emit Finished();
}

void CategoryGrabber::Search(const std::string &)
{
	static const std::string uri = "https://api.twitch.tv";
	const std::string path = "/helix/search/categories";

	int startCount = _categoryMap.size();
	std::string cursor;
	httplib::Params params = {
		{"first", "100"}, {"after", cursor}, {"query", _searchString}};
	auto response = SendGetRequest(*_token, uri, path, params);

	while (response.status == 200 && !_stop) {
		cursor = ParseReply(response.data);
		if (cursor.empty()) {
			break; // End of category list
		}
		params = {{"first", "100"},
			  {"after", cursor},
			  {"query", _searchString}};
		response = SendGetRequest(*_token, uri, path, params);
		emit CategoryCountUpdated(_categoryMap.size() - startCount);
	}
}

void CategoryGrabber::GetAll()
{
	static const std::string uri = "https://api.twitch.tv";
	const std::string path = "/helix/games/top";

	// Declare static to "save" progress in case of cancel
	static std::string cursor;

	httplib::Params params = {{"first", "100"}, {"after", cursor}};
	auto response = SendGetRequest(*_token, uri, path, params);

	while (response.status == 200 && !_stop) {
		cursor = ParseReply(response.data);
		if (cursor.empty()) {
			break; // End of category list
		}
		params = {{"first", "100"}, {"after", cursor}};
		response = SendGetRequest(*_token, uri, path, params);
		emit CategoryCountUpdated(_categoryMap.size());
	}
}

std::string CategoryGrabber::ParseReply(obs_data_t *data) const
{
	OBSDataArrayAutoRelease array = obs_data_get_array(data, "data");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(array, i);
		int id = std::stoi(obs_data_get_string(arrayObj, "id"));
		QString name = obs_data_get_string(arrayObj, "name");
		_categoryMap.emplace(name, id);
	}
	OBSDataAutoRelease pagination = obs_data_get_obj(data, "pagination");
	return obs_data_get_string(pagination, "cursor");
}

ProgressDialog::ProgressDialog(QWidget *parent, bool showSkip)
	: QDialog(parent),
	  _skipFetchCheckBox(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.twitchCategories.fetchSkip"))),
	  _status(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.twitchCategories.fetchStart")))
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	auto layout = new QVBoxLayout(this);
	layout->addWidget(_status);
	auto cancelButton = new QPushButton(
		obs_module_text("AdvSceneSwitcher.twitchCategories.fetchStop"),
		this);
	layout->addWidget(_skipFetchCheckBox);
	layout->addWidget(cancelButton);
	setLayout(layout);

	QWidget::connect(_skipFetchCheckBox, &QCheckBox::stateChanged, this,
			 [this](int value) { _skipFetch = value; });
	QWidget::connect(cancelButton, &QPushButton::clicked, this,
			 [this]() { _skipFetch ? accept() : reject(); });

	_skipFetchCheckBox->setVisible(showSkip);
	if (_skipFetch) {
		accept();
	}
}

void ProgressDialog::CategoryCountUpdated(int value)
{
	_status->setText(
		QString(obs_module_text(
				"AdvSceneSwitcher.twitchCategories.fetchStatus"))
			.arg(value));
}

TwitchCategorySearchButton::TwitchCategorySearchButton(QWidget *parent)
	: QPushButton(parent)
{
	setMaximumWidth(22);
	const std::string pathPrefix =
		GetDataFilePath("res/images/" + GetThemeTypeName());
	SetButtonIcon(this, (pathPrefix + "Search.svg").c_str());
	setToolTip(obs_module_text(
		"AdvSceneSwitcher.twitchCategories.manualSearch"));
	QWidget::connect(this, SIGNAL(clicked()), this,
			 SLOT(StartManualCategorySearch()));
	QWidget::connect(this, SIGNAL(RequestRepopulate()),
			 TwitchCategorySignalManager::Instance(),
			 SIGNAL(RepopulateRequired()));
}

void TwitchCategorySearchButton::SetToken(
	const std::weak_ptr<TwitchToken> &token)
{
	_token = token;
	const bool expired = token.expired();
	setDisabled(expired);
	if (expired) {
		setToolTip(obs_module_text(
			"AdvSceneSwitcher.action.twitch.categorySelectionDisabled"));
	} else {
		setToolTip(obs_module_text(
			"AdvSceneSwitcher.twitchCategories.manualSearch"));
	}
}

void TwitchCategorySearchButton::StartManualCategorySearch()
{
	std::string category;
	bool accepted = AdvSSNameDialog::AskForName(
		this,
		obs_module_text("AdvSceneSwitcher.twitchCategories.search"),
		obs_module_text("AdvSceneSwitcher.twitchCategories.name"),
		category);
	if (!accepted) {
		return;
	}

	CategoryGrabber categoryGrabber;
	auto *progressDialog = new ProgressDialog(this, false);

	QWidget::connect(&categoryGrabber, SIGNAL(CategoryCountUpdated(int)),
			 progressDialog, SLOT(CategoryCountUpdated(int)));
	QWidget::connect(&categoryGrabber, &CategoryGrabber::Finished, this,
			 [progressDialog]() { progressDialog->accept(); });
	QWidget::connect(&categoryGrabber, &CategoryGrabber::Failed, this,
			 [progressDialog]() { progressDialog->reject(); });

	auto previousCategoryCount = categoryGrabber.GetCategories().size();

	categoryGrabber.Start(_token.lock(), category);
	progressDialog->exec();
	categoryGrabber.Stop();
	categoryGrabber.wait();

	emit RequestRepopulate();
	progressDialog->deleteLater();

	auto newCategoryCount =
		categoryGrabber.GetCategories().size() - previousCategoryCount;
	if (newCategoryCount == 0) {
		DisplayMessage(
			QString(obs_module_text(
					"AdvSceneSwitcher.twitchCategories.searchFailed"))
				.arg(QString::fromStdString(category)));
	} else {
		DisplayMessage(
			QString(obs_module_text(
					"AdvSceneSwitcher.twitchCategories.searchSuccess"))
				.arg(QString::number(newCategoryCount),
				     QString::fromStdString(category)));
	}
}

TwitchCategoryWidget::TwitchCategoryWidget(QWidget *parent)
	: QWidget(parent),
	  _selection(new TwitchCategorySelection(this)),
	  _manualSearch(new TwitchCategorySearchButton(this))
{
	QWidget::connect(_selection,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)), this,
			 SIGNAL(CategoreyChanged(const TwitchCategory &)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_selection);
	layout->addWidget(_manualSearch);
	setLayout(layout);
}

void TwitchCategoryWidget::SetCategory(const TwitchCategory &category)
{
	_selection->SetCategory(category);
}

void TwitchCategoryWidget::SetToken(const std::weak_ptr<TwitchToken> &token)
{
	_selection->SetToken(token);
	_manualSearch->SetToken(token);
}

TwitchCategorySignalManager *TwitchCategorySignalManager::Instance()
{
	static TwitchCategorySignalManager manager;
	return &manager;
}

} // namespace advss
