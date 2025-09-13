#pragma once
#include "variable-line-edit.hpp"
#include "token.hpp"

namespace advss {

struct LanguageSelection {
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	void SetStreamLanguage(const TwitchToken &) const;

private:
	StringVariable _language = "en";
	bool _useManualInput = false;
	friend class LanguageSelectionWidget;
};

class LanguageSelectionWidget final : public QWidget {
	Q_OBJECT

public:
	explicit LanguageSelectionWidget(QWidget *parent = nullptr);
	void SetLanguageSelection(const LanguageSelection &);
	void SetToken(const std::weak_ptr<TwitchToken> &);

private slots:
	void GetCurrentClicked();
	void TextChanged();
	void ComboSelectionChanged();
signals:
	void LanguageChanged(const LanguageSelection &);

private:
	VariableLineEdit *_languageText;
	QComboBox *_languageCombo;
	QPushButton *_toggleInput;

	std::weak_ptr<TwitchToken> _token;
};

} // namespace advss
