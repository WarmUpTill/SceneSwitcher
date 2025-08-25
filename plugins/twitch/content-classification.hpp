#pragma once
#include "token.hpp"

#include <QWidget>

class QCheckBox;

namespace advss {

struct ContentClassification {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	void SetContentClassification(const TwitchToken &token) const;

private:
	bool _debatedSocialIssuesAndPolitics = false;
	bool _drugsIntoxication = false;
	bool _sexualThemes = false;
	bool _violentGraphic = false;
	bool _gambling = false;
	bool _profanityVulgarity = false;

	friend class ContentClassificationEdit;
};

class ContentClassificationEdit final : public QWidget {
	Q_OBJECT

public:
	explicit ContentClassificationEdit(QWidget *parent = nullptr);
	void SetContentClassification(const ContentClassification &ccl);
	void SetToken(const std::weak_ptr<TwitchToken> &token);

private slots:
	void GetCurrentClicked();
signals:
	void ContentClassificationChanged(const ContentClassification &);

private:
	QCheckBox *_debatedSocialIssuesAndPolitics;
	QCheckBox *_drugsIntoxication;
	QCheckBox *_sexualThemes;
	QCheckBox *_violentGraphic;
	QCheckBox *_gambling;
	QCheckBox *_profanityVulgarity;

	std::weak_ptr<TwitchToken> _token;
};

} // namespace advss
