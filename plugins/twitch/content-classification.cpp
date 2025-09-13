#include "content-classification.hpp"
#include "channel-selection.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "twitch-helpers.hpp"

#include <QPushButton>
#include <QVBoxLayout>

namespace advss {

void ContentClassification::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data =
		obs_data_get_obj(obj, "contentClassificationLabels");
	_debatedSocialIssuesAndPolitics =
		obs_data_get_bool(data, "debatedSocialIssuesAndPolitics");
	_drugsIntoxication = obs_data_get_bool(data, "drugsIntoxication");
	_sexualThemes = obs_data_get_bool(data, "sexualThemes");
	_violentGraphic = obs_data_get_bool(data, "violentGraphic");
	_gambling = obs_data_get_bool(data, "gambling");
	_profanityVulgarity = obs_data_get_bool(data, "profanityVulgarity");
}

void ContentClassification::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "debatedSocialIssuesAndPolitics",
			  _debatedSocialIssuesAndPolitics);
	obs_data_set_bool(data, "drugsIntoxication", _drugsIntoxication);
	obs_data_set_bool(data, "sexualThemes", _sexualThemes);
	obs_data_set_bool(data, "violentGraphic", _violentGraphic);
	obs_data_set_bool(data, "gambling", _gambling);
	obs_data_set_bool(data, "profanityVulgarity", _profanityVulgarity);
	obs_data_set_obj(obj, "contentClassificationLabels", data);
}

void ContentClassification::SetContentClassification(
	const TwitchToken &token) const
{
	OBSDataArrayAutoRelease ccls = obs_data_array_create();

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", "DebatedSocialIssuesAndPolitics");
	obs_data_set_bool(data, "is_enabled", _debatedSocialIssuesAndPolitics);
	obs_data_array_push_back(ccls, data);
	data = obs_data_create();
	obs_data_set_string(data, "id", "DrugsIntoxication");
	obs_data_set_bool(data, "is_enabled", _drugsIntoxication);
	obs_data_array_push_back(ccls, data);
	data = obs_data_create();
	obs_data_set_string(data, "id", "SexualThemes");
	obs_data_set_bool(data, "is_enabled", _sexualThemes);
	obs_data_array_push_back(ccls, data);
	data = obs_data_create();
	obs_data_set_string(data, "id", "ViolentGraphic");
	obs_data_set_bool(data, "is_enabled", _violentGraphic);
	obs_data_array_push_back(ccls, data);
	data = obs_data_create();
	obs_data_set_string(data, "id", "Gambling");
	obs_data_set_bool(data, "is_enabled", _gambling);
	obs_data_array_push_back(ccls, data);
	data = obs_data_create();
	obs_data_set_string(data, "id", "ProfanityVulgarity");
	obs_data_set_bool(data, "is_enabled", _profanityVulgarity);
	obs_data_array_push_back(ccls, data);

	data = obs_data_create();
	obs_data_set_array(data, "content_classification_labels", ccls);

	auto result = SendPatchRequest(token, "https://api.twitch.tv",
				       "/helix/channels",
				       {{"broadcaster_id", token.GetUserID()}},
				       data.Get());

	if (result.status != 204) {
		blog(LOG_INFO,
		     "Failed to set stream content classification labels! (%d)",
		     result.status);
	}
}

ContentClassificationEdit::ContentClassificationEdit(QWidget *parent)
	: QWidget(parent),
	  _debatedSocialIssuesAndPolitics(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.twitch.contentClassification.debatedSocialIssuesAndPolitics"),
		  this)),
	  _drugsIntoxication(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.twitch.contentClassification.drugsIntoxication"),
		  this)),
	  _sexualThemes(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.twitch.contentClassification.sexualThemes"),
		  this)),
	  _violentGraphic(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.twitch.contentClassification.violentGraphic"),
		  this)),
	  _gambling(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.twitch.contentClassification.gambling"),
		  this)),
	  _profanityVulgarity(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.twitch.contentClassification.profanityVulgarity"),
		  this))
{
	auto getCurrent = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.action.twitch.contentClassification.getCurrent"));
	connect(getCurrent, &QPushButton::clicked, this,
		&ContentClassificationEdit::GetCurrentClicked);

	const auto emitChangeSignal = [this]() {
		ContentClassification ccl;
		ccl._debatedSocialIssuesAndPolitics =
			_debatedSocialIssuesAndPolitics->isChecked();
		ccl._drugsIntoxication = _drugsIntoxication->isChecked();
		ccl._sexualThemes = _sexualThemes->isChecked();
		ccl._violentGraphic = _violentGraphic->isChecked();
		ccl._gambling = _gambling->isChecked();
		ccl._profanityVulgarity = _profanityVulgarity->isChecked();
		emit ContentClassificationChanged(ccl);
	};

	connect(_debatedSocialIssuesAndPolitics, &QCheckBox::stateChanged, this,
		emitChangeSignal);
	connect(_drugsIntoxication, &QCheckBox::stateChanged, this,
		emitChangeSignal);
	connect(_sexualThemes, &QCheckBox::stateChanged, this,
		emitChangeSignal);
	connect(_violentGraphic, &QCheckBox::stateChanged, this,
		emitChangeSignal);
	connect(_gambling, &QCheckBox::stateChanged, this, emitChangeSignal);
	connect(_profanityVulgarity, &QCheckBox::stateChanged, this,
		emitChangeSignal);

	auto layout = new QVBoxLayout;
	layout->addWidget(_debatedSocialIssuesAndPolitics);
	layout->addWidget(_drugsIntoxication);
	layout->addWidget(_sexualThemes);
	layout->addWidget(_violentGraphic);
	layout->addWidget(_gambling);
	layout->addWidget(_profanityVulgarity);
	layout->addWidget(getCurrent);
	setLayout(layout);

	// TODO:
	// Figure out why the Twitch API always returns an empty CC list
	// Hide for now
	getCurrent->hide();
}

void ContentClassificationEdit::SetContentClassification(
	const ContentClassification &ccl)
{
	_debatedSocialIssuesAndPolitics->setChecked(
		ccl._debatedSocialIssuesAndPolitics);
	_drugsIntoxication->setChecked(ccl._drugsIntoxication);
	_sexualThemes->setChecked(ccl._sexualThemes);
	_violentGraphic->setChecked(ccl._violentGraphic);
	_gambling->setChecked(ccl._gambling);
	_profanityVulgarity->setChecked(ccl._profanityVulgarity);
}

void ContentClassificationEdit::SetToken(const std::weak_ptr<TwitchToken> &t)
{
	_token = t;
}

void ContentClassificationEdit::GetCurrentClicked()
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

	ContentClassification ccl;
	for (const auto &label : channelInfo->content_classification_labels) {
		if (label == "DebatedSocialIssuesAndPolitics") {
			ccl._debatedSocialIssuesAndPolitics = true;
		} else if (label == "DrugsIntoxication") {
			ccl._drugsIntoxication = true;
		} else if (label == "SexualThemes") {
			ccl._sexualThemes = true;
		} else if (label == "ViolentGraphic") {
			ccl._violentGraphic = true;
		} else if (label == "Gambling") {
			ccl._gambling = true;
		} else if (label == "ProfanityVulgarity") {
			ccl._profanityVulgarity = true;
		}
	}

	SetContentClassification(ccl);
}

} // namespace advss
