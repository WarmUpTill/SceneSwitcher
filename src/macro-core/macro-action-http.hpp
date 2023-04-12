#pragma once
#include "macro-action-edit.hpp"
#include "variable-text-edit.hpp"
#include "variable-line-edit.hpp"
#include "duration-control.hpp"
#include "string-list.hpp"

#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

namespace advss {

class MacroActionHttp : public MacroAction {
public:
	MacroActionHttp(Macro *m) : MacroAction(m, true) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionHttp>(m);
	}

	enum class Method {
		GET = 0,
		POST,
	};

	StringVariable _url = obs_module_text("AdvSceneSwitcher.enterURL");
	StringVariable _data = obs_module_text("AdvSceneSwitcher.enterText");
	bool _setHeaders = false;
	StringList _headers;
	Method _method = Method::GET;
	Duration _timeout = Duration(1.0);

private:
	void SetupHeaders();
	void Get();
	void Post();

	static bool _registered;
	static const std::string id;
};

class MacroActionHttpEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionHttpEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionHttp> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionHttpEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionHttp>(action));
	}

private slots:
	void DataChanged();
	void URLChanged();
	void MethodChanged(int);
	void TimeoutChanged(const Duration &seconds);
	void SetHeadersChanged(int);
	void HeadersChanged(const StringList &);
signals:
	void HeaderInfoChanged(const QString &);

protected:
	std::shared_ptr<MacroActionHttp> _entryData;

private:
	void SetWidgetVisibility();

	VariableLineEdit *_url;
	QComboBox *_methods;
	VariableTextEdit *_data;
	QCheckBox *_setHeaders;
	QVBoxLayout *_headerListLayout;
	StringListEdit *_headerList;
	DurationSelection *_timeout;
	bool _loading = true;
};

} // namespace advss
