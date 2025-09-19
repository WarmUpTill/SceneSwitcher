#include "tag-selection.hpp"
#include "channel-selection.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "twitch-helpers.hpp"

#include <nlohmann/json.hpp>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace advss {

void TwitchTagList::Load(obs_data_t *obj)
{
	StringList::Load(obj, "tags", "tag");
}

void TwitchTagList::Save(obs_data_t *obj) const
{
	StringList::Save(obj, "tags", "tag");
}

void TwitchTagList::SetStreamTags(const TwitchToken &token) const
{
	nlohmann::json j;
	j["tags"] = toVector();

	// Although the Twitch API doesn't mention it, a request only containing
	// an empty tags array will be denied.
	// So, we just set the title, too, to work around this limitation.
	if (isEmpty()) {
		TwitchChannel channel;
		channel.SetName(token.Name());
		const auto channelInfo = channel.GetInfo(token);
		if (!channelInfo) {
			return;
		}
		if (channelInfo->title.empty()) {
			return;
		}
		j["title"] = channelInfo->title;
	}

	auto result = SendPatchRequest(token, "https://api.twitch.tv",
				       "/helix/channels",
				       {{"broadcaster_id", token.GetUserID()}},
				       j.dump());

	if (result.status != 204) {
		blog(LOG_INFO, "Failed to set stream tags! (%d)",
		     result.status);
	}
}

TagListWidget::TagListWidget(QWidget *parent)
	: StringListEdit(
		  parent,
		  obs_module_text("AdvSceneSwitcher.action.twitch.tags.add"),
		  obs_module_text("AdvSceneSwitcher.action.twitch.tags.add"),
		  _maxTagLength,
		  [this](const std::string &input) { return Filter(input); },
		  [](std::string &input) {
			  input = QString::fromStdString(input)
					  .trimmed()
					  .toStdString();
		  })
{
	connect(this, SIGNAL(StringListChanged(const StringList &)), this,
		SLOT(StringListChangedWrapper(const StringList &)));

	auto getCurrent = new QPushButton(obs_module_text(
		"AdvSceneSwitcher.action.twitch.tags.getCurrent"));
	connect(getCurrent, &QPushButton::clicked, this,
		&TagListWidget::GetCurrentClicked);
	_mainLayout->addWidget(getCurrent);

	// TODO:
	// Figure out why the Twitch API always returns an empty tag list
	// Hide for now
	getCurrent->hide();
}

void TagListWidget::SetTags(const TwitchTagList &tags)
{
	SetStringList(tags);
}

void TagListWidget::SetToken(const std::weak_ptr<TwitchToken> &t)
{
	_token = t;
}

void TagListWidget::GetCurrentClicked()
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

	TwitchTagList tags;
	for (const auto &tag : channelInfo->tags) {
		tags << tag;
	}

	SetTags(tags);
}

void TagListWidget::StringListChangedWrapper(const StringList &list)
{
	emit TagListChanged(TwitchTagList{list});
}

bool TagListWidget::Filter(const std::string &input)
{
	const auto tag = QString::fromStdString(input);

	if (!IsValidTag(tag)) {
		QMessageBox::warning(
			this,
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.tags.invalid"),
			QString(obs_module_text(
					"AdvSceneSwitcher.action.twitch.tags.invalid.info"))
				.arg(_maxTagLength));
		return true;
	}

	if (ContainsTag(tag)) {
		QMessageBox::information(
			this,
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.tags.duplicate"),
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.tags.duplicate.info"));
		return true;
	}

	if (count() >= _maxTags) {
		QMessageBox::warning(
			this,
			obs_module_text(
				"AdvSceneSwitcher.action.twitch.tags.limit"),
			QString(obs_module_text(
					"AdvSceneSwitcher.action.twitch.tags.limit.info"))
				.arg(_maxTags));
		return true;
	}

	return false;
}

bool TagListWidget::IsValidTag(const QString &tag)
{
	if (tag.isEmpty() || tag.length() > _maxTagLength) {
		return false;
	}

	// If string contains variables don't filter
	const StringVariable tmp = tag.toStdString();
	if (tmp.UnresolvedValue() != std::string(tmp)) {
		return true;
	}

	QRegularExpression regex("^[A-Za-z0-9]+$"); // Only letters and digits
	return regex.match(tag).hasMatch();
}

bool TagListWidget::ContainsTag(const QString &tag) const
{
	const auto lowerTag = tag.toLower();
	const auto tags = GetStringList();
	for (const auto &tag : tags) {
		if (QString::fromStdString(tag.UnresolvedValue()).toLower() ==
		    lowerTag) {
			return true;
		}
	}
	return false;
}

} // namespace advss
