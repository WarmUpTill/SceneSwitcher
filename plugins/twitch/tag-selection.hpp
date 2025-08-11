#pragma once
#include "variable-line-edit.hpp"
#include "string-list.hpp"
#include "token.hpp"

#include <QWidget>
#include <string>
#include <vector>

class QListWidget;
class QPushButton;
class TwitchToken;

namespace advss {

struct TwitchTagList : public StringList {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	void SetStreamTags(const TwitchToken &token) const;
};

class TagListWidget final : public StringListEdit {
	Q_OBJECT

public:
	explicit TagListWidget(QWidget *parent = nullptr);
	void SetTags(const TwitchTagList &tags);
	void SetToken(const std::weak_ptr<TwitchToken> &token);

private slots:
	void StringListChangedWrapper(const StringList &);
	void GetCurrentClicked();
signals:
	void TagListChanged(const TwitchTagList &);

private:
	bool Filter(const std::string &input);
	static bool IsValidTag(const QString &tag);
	bool ContainsTag(const QString &tag) const;

	std::weak_ptr<TwitchToken> _token;

	static constexpr int _maxTags = 10;
	static constexpr int _maxTagLength = 25;
};

} // namespace advss
