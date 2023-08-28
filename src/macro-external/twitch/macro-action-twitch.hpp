#pragma once
#include "macro-action-edit.hpp"
#include "token.hpp"
#include "category-selection.hpp"

#include <variable-line-edit.hpp>
#include <duration-control.hpp>

namespace advss {

class MacroActionTwitch : public MacroAction {
public:
	MacroActionTwitch(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionTwitch>(m);
	}

	enum class Action {
		TITLE,
		CATEGORY,
		COMMERCIAL,
	};

	Action _action = Action::TITLE;
	std::weak_ptr<TwitchToken> _token;
	StringVariable _text = obs_module_text("AdvSceneSwitcher.enterText");
	TwitchCategory _category;
	Duration _duration = 60;

private:
	void SetStreamTitle(const std::shared_ptr<TwitchToken> &) const;
	void SetStreamCategory(const std::shared_ptr<TwitchToken> &) const;
	void StartCommercial(const std::shared_ptr<TwitchToken> &) const;

	static bool _registered;
	static const std::string id;
};

class MacroActionTwitchEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionTwitchEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionTwitch> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionTwitchEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionTwitch>(action));
	}

private slots:
	void ActionChanged(int);
	void TwitchTokenChanged(const QString &);
	void TextChanged();
	void CategoreyChanged(const TwitchCategory &);
	void DurationChanged(const Duration &);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionTwitch> _entryData;

private:
	void SetupWidgetVisibility();

	QComboBox *_actions;
	TwitchConnectionSelection *_tokens;
	VariableLineEdit *_text;
	TwitchCategorySelection *_category;
	TwitchCategorySearchButton *_manualCategorySearch;
	DurationSelection *_duration;
	QHBoxLayout *_layout;
	bool _loading = true;
};

} // namespace advss
