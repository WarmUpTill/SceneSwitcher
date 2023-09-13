#include "channel-selection.hpp"
#include "twitch-helpers.hpp"
#include "token.hpp"

#include <QLayout>
#include <QDesktopServices>
#include <obs-module-helper.hpp>

namespace advss {

void TwitchChannel::Load(obs_data_t *obj)
{
	OBSDataAutoRelease data = obs_data_get_obj(obj, "category");
	_id = obs_data_get_string(data, "id");
	_name.Load(obj, "name");
}

void TwitchChannel::Save(obs_data_t *obj) const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "id", _id.c_str());
	_name.Save(obj, "name");
}

std::string TwitchChannel::GetUserID(const TwitchToken &token)
{
	if (std::string(_name) == _lastResolvedValue) {
		return _id;
	}
	_lastResolvedValue = _name;
	auto res = SendGetRequest("https://api.twitch.tv", "/helix/users",
				  token, {{"login", _name}});
	if (res.status != 200) {
		blog(LOG_WARNING, "failed to get Twitch user id from token!");
		_id = "invalid";
		return _id;
	}
	OBSDataArrayAutoRelease array = obs_data_get_array(res.data, "data");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(array, i);
		_id = obs_data_get_string(arrayObj, "id");
	}
	return _id;
}

TwitchChannelSelection::TwitchChannelSelection(QWidget *parent)
	: QWidget(parent),
	  _channelName(new VariableLineEdit(this)),
	  _openChannel(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.channel.open")))
{
	QWidget::connect(_channelName, SIGNAL(editingFinished()), this,
			 SLOT(SelectionChanged()));
	QWidget::connect(_openChannel, SIGNAL(pressed()), this,
			 SLOT(OpenChannel()));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_channelName);
	layout->addWidget(_openChannel);
	setLayout(layout);
}

void TwitchChannelSelection::SetChannel(const TwitchChannel &channel)
{
	_channelName->setText(channel._name);
}

void TwitchChannelSelection::OpenChannel()
{
	// Resolve potential variables
	StringVariable temp = _channelName->text().toStdString();
	QDesktopServices::openUrl("https://www.twitch.tv/" +
				  QString::fromStdString(temp));
}

void TwitchChannelSelection::SelectionChanged()
{
	TwitchChannel channel;
	channel._name = _channelName->text().toStdString();
	emit ChannelChanged(channel);
}

} // namespace advss
