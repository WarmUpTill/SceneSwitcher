#pragma once
#include "macro-action-edit.hpp"
#include "key-value-list.hpp"
#include "variable-text-edit.hpp"
#include "variable-line-edit.hpp"
#include "duration-control.hpp"

#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

namespace advss {

class MacroActionHttp final : public MacroAction {
public:
	MacroActionHttp(Macro *m) : MacroAction(m, true) {}
	bool PerformAction();
	void LogAction() const;
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Method {
		GET = 0,
		POST,
		PUT,
		PATCH,
		DELETE,
	};

	StringVariable _url = "127.0.0.1:8080";
	StringVariable _path = "/";
	StringVariable _body = obs_module_text("AdvSceneSwitcher.enterText");
	StringVariable _contentType = "application/json";
	bool _setHeaders = false;
	StringList _headers;
	bool _setParams = false;
	StringList _params;
	Method _method = Method::GET;
	Duration _timeout = Duration(1.0);

private:
	void SetupTempVars();

	static bool _registered;
	static const std::string id;
};

class MacroActionHttpEdit final : public QWidget {
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
	void URLChanged();
	void PathChanged();
	void BodyChanged();
	void ContentTypeChanged();
	void MethodChanged(int);
	void TimeoutChanged(const Duration &seconds);
	void SetHeadersChanged(int);
	void HeadersChanged(const StringList &);
	void SetParamsChanged(int);
	void ParamsChanged(const StringList &);
signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetSignalConnections();
	void SetWidgetLayout();
	void SetWidgetVisibility();

	VariableLineEdit *_url;
	VariableLineEdit *_path;
	VariableLineEdit *_contentType;
	QHBoxLayout *_contentTypeLayout;
	QComboBox *_methods;
	VariableTextEdit *_body;
	QVBoxLayout *_bodyLayout;
	QCheckBox *_setHeaders;
	KeyValueListEdit *_headerList;
	QVBoxLayout *_headerListLayout;
	QCheckBox *_setParams;
	KeyValueListEdit *_paramList;
	QVBoxLayout *_paramListLayout;
	DurationSelection *_timeout;

	std::shared_ptr<MacroActionHttp> _entryData;
	bool _loading = true;
};

} // namespace advss
