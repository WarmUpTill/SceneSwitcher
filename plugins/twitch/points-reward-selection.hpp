#pragma once
#include "channel-selection.hpp"
#include "token.hpp"

#include <filter-combo-box.hpp>
#include <obs-data.h>
#include <string>

namespace advss {

struct TwitchPointsReward {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;

	std::string id = "-";
	std::string title = "-";
};

class TwitchPointsRewardSelection : public FilterComboBox {
	Q_OBJECT

public:
	TwitchPointsRewardSelection(QWidget *parent, bool allowAny = true);

	void SetPointsReward(const TwitchPointsReward &pointsReward);
	void SetChannel(const TwitchChannel &channel);
	void SetToken(const std::weak_ptr<TwitchToken> &token);

protected:
	void DisplayErrorMessage(const char *errorMessage);
	void HideErrorMessage();

	std::optional<TwitchChannel> _channel = {};
	std::weak_ptr<TwitchToken> _token;

private:
	void AddPredefinedItems();
	std::optional<std::vector<TwitchPointsReward>>
	GetPointsRewards(const std::shared_ptr<TwitchToken> &token,
			 const TwitchChannel &channel);
	std::vector<TwitchPointsReward> ParseResponse(obs_data_t *response);

	bool _allowAny;
	const std::vector<TokenOption> SUPPORTED_TOKEN_OPTIONS = {
		{"channel:read:redemptions"},
		{"channel:manage:redemptions"}};

private slots:
	void PopulateSelection();
	void SelectionChanged(int);

signals:
	void PointsRewardChanged(const TwitchPointsReward &);
};

class TwitchPointsRewardWidget : public QWidget {
	Q_OBJECT

public:
	TwitchPointsRewardWidget(QWidget *parent);

	void SetPointsReward(const TwitchPointsReward &pointsReward);
	void SetChannel(const TwitchChannel &channel);
	void SetToken(const std::weak_ptr<TwitchToken> &token);

signals:
	void PointsRewardChanged(const TwitchPointsReward &);

protected:
	TwitchPointsRewardSelection *_selection;
	QPushButton *_refreshButton;
};

} // namespace advss
