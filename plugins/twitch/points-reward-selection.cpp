#include "points-reward-selection.hpp"
#include "twitch-helpers.hpp"

#include <obs-module-helper.hpp>
#include <ui-helpers.hpp>

namespace advss {

void TwitchPointsReward::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "pointsReward");
	id = obs_data_get_string(data, "id");
	title = obs_data_get_string(data, "title");
}

void TwitchPointsReward::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", id.c_str());
	obs_data_set_string(data, "title", title.c_str());
	obs_data_set_obj(obj, "pointsReward", data);
}

TwitchPointsRewardSelection::TwitchPointsRewardSelection(QWidget *parent,
							 bool allowAny)
	: FilterComboBox(
		  parent,
		  obs_module_text(
			  "AdvSceneSwitcher.twitch.selection.points.reward.placeholder")),
	  _allowAny(allowAny)
{
	setDuplicatesEnabled(true);
	setSizeAdjustPolicy(QComboBox::AdjustToContents);

	QWidget::connect(this, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));
}

void TwitchPointsRewardSelection::SetPointsReward(
	const TwitchPointsReward &pointsReward)
{
	int index = findData(QString::fromStdString(pointsReward.id));
	if (index != -1) {
		setCurrentIndex(index);
		return;
	}

	setCurrentIndex(-1);
}

void TwitchPointsRewardSelection::SetChannel(const TwitchChannel &channel)
{
	_channel = channel;
	PopulateSelection();
}

void TwitchPointsRewardSelection::SetToken(
	const std::weak_ptr<TwitchToken> &token)
{
	_token = token;

	if (token.expired()) {
		DisplayErrorMessage(obs_module_text(
			"AdvSceneSwitcher.twitch.selection.points.reward.tooltip.noAccount"));
		return;
	}

	PopulateSelection();
}

void TwitchPointsRewardSelection::PopulateSelection()
{
	auto token = _token.lock();

	if (!token || !token->AnyOptionIsEnabled(SUPPORTED_TOKEN_OPTIONS)) {
		DisplayErrorMessage(obs_module_text(
			"AdvSceneSwitcher.twitch.selection.points.reward.tooltip.noPermission"));
		return;
	}

	if (!_channel || (*_channel).GetName().empty()) {
		DisplayErrorMessage(obs_module_text(
			"AdvSceneSwitcher.twitch.selection.points.reward.tooltip.noChannel"));
		return;
	}

	auto currentSelection = currentText();
	const QSignalBlocker b(this);
	clear();

	auto pointsRewards = GetPointsRewards(token, *_channel);
	if (!pointsRewards) {
		DisplayErrorMessage(obs_module_text(
			"AdvSceneSwitcher.twitch.selection.points.reward.tooltip.error"));
		return;
	}

	HideErrorMessage();
	AddPredefinedItems();

	for (const auto &pointsReward : *pointsRewards) {
		addItem(QString::fromStdString(pointsReward.title),
			QString::fromStdString(pointsReward.id));
	}

	setCurrentText(currentSelection);
}

void TwitchPointsRewardSelection::AddPredefinedItems()
{
	if (_allowAny) {
		addItem(obs_module_text(
				"AdvSceneSwitcher.twitch.selection.points.reward.option.any"),
			"-");
	}
}

void TwitchPointsRewardSelection::DisplayErrorMessage(const char *errorMessage)
{
	setDisabled(true);
	setToolTip(errorMessage);
}

void TwitchPointsRewardSelection::HideErrorMessage()
{
	setDisabled(false);
	setToolTip("");
}

std::optional<std::vector<TwitchPointsReward>>
TwitchPointsRewardSelection::GetPointsRewards(
	const std::shared_ptr<TwitchToken> &token, const TwitchChannel &channel)
{
	httplib::Params params = {
		{"broadcaster_id", channel.GetUserID(*token)}};

	auto response = SendGetRequest(*token, "https://api.twitch.tv",
				       "/helix/channel_points/custom_rewards",
				       params);

	if (response.status != 200) {
		blog(LOG_WARNING,
		     "Failed to fetch points rewards for user %s and channel %s! (%d)",
		     token->GetName().c_str(), channel.GetName().c_str(),
		     response.status);

		return {};
	}

	return ParseResponse(response.data);
}

std::vector<TwitchPointsReward>
TwitchPointsRewardSelection::ParseResponse(obs_data_t *response)
{
	std::vector<TwitchPointsReward> pointRewards;
	OBSDataArrayAutoRelease jsonArray =
		obs_data_get_array(response, "data");
	size_t count = obs_data_array_count(jsonArray);

	for (size_t i = 0; i < count; ++i) {
		OBSDataAutoRelease jsonObj = obs_data_array_item(jsonArray, i);
		std::string id = obs_data_get_string(jsonObj, "id");
		std::string title = obs_data_get_string(jsonObj, "title");
		pointRewards.push_back({id, title});
	}

	return pointRewards;
}

void TwitchPointsRewardSelection::SelectionChanged(int index)
{
	TwitchPointsReward pointsReward{
		itemData(index).toString().toStdString(),
		currentText().toStdString()};
	emit PointsRewardChanged(pointsReward);
}

TwitchPointsRewardWidget::TwitchPointsRewardWidget(QWidget *parent)
	: QWidget(parent),
	  _selection(new TwitchPointsRewardSelection(this)),
	  _refreshButton(new QPushButton(this))
{
	_refreshButton->setMaximumWidth(22);
	SetButtonIcon(_refreshButton, ":res/images/refresh.svg");
	_refreshButton->setToolTip(obs_module_text(
		"AdvSceneSwitcher.twitch.selection.points.reward.refresh"));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_selection);
	layout->addWidget(_refreshButton);
	setLayout(layout);

	QWidget::connect(
		_selection,
		SIGNAL(PointsRewardChanged(const TwitchPointsReward &)), this,
		SIGNAL(PointsRewardChanged(const TwitchPointsReward &)));
	QWidget::connect(_refreshButton, SIGNAL(clicked()), _selection,
			 SLOT(PopulateSelection()));
}

void TwitchPointsRewardWidget::SetPointsReward(
	const TwitchPointsReward &pointsReward)
{
	_selection->SetPointsReward(pointsReward);
}

void TwitchPointsRewardWidget::SetChannel(const TwitchChannel &channel)
{
	_selection->SetChannel(channel);
}

void TwitchPointsRewardWidget::SetToken(const std::weak_ptr<TwitchToken> &token)
{
	_selection->SetToken(token);
}

} // namespace advss
