#pragma once
#include "macro-condition-edit.hpp"
#include "http-server.hpp"
#include "variable-line-edit.hpp"
#include "variable-text-edit.hpp"
#include "regex-config.hpp"

#include <QCheckBox>
#include <QComboBox>

namespace advss {

class MacroConditionHttp : public MacroCondition {
public:
	MacroConditionHttp(Macro *m) : MacroCondition(m, true) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; }
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionHttp>(m);
	}

	enum class Method {
		ANY = 0,
		GET,
		POST,
		PUT,
		PATCH,
		DELETE,
	};

	void SetServer(const std::string &name);
	std::weak_ptr<HttpServer> GetServer() const { return _server; }

	StringVariable _path = ".*";
	StringVariable _body = ".*";
	RegexConfig _pathRegex = RegexConfig::PartialMatchRegexConfig(true);
	RegexConfig _bodyRegex = RegexConfig::PartialMatchRegexConfig(true);
	Method _method = Method::ANY;
	bool _clearBufferOnMatch = true;

private:
	void SetupTempVars();

	std::weak_ptr<HttpServer> _server;
	HttpRequestBuffer _requestBuffer;
	std::chrono::high_resolution_clock::time_point _lastCheck{};

	static bool _registered;
	static const std::string id;
};

class MacroConditionHttpEdit : public QWidget {
	Q_OBJECT
public:
	MacroConditionHttpEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionHttp> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionHttpEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionHttp>(cond));
	}

private slots:
	void MethodChanged(int);
	void ServerChanged(const QString &);
	void PathChanged();
	void PathRegexChanged(const RegexConfig &);
	void BodyChanged();
	void BodyRegexChanged(const RegexConfig &);
	void ClearBufferOnMatchChanged(int);
signals:
	void HeaderInfoChanged(const QString &);

private:
	QComboBox *_method;
	HttpServerSelection *_server;
	VariableLineEdit *_path;
	RegexConfigWidget *_pathRegex;
	VariableTextEdit *_body;
	RegexConfigWidget *_bodyRegex;
	QCheckBox *_clearBufferOnMatch;

	std::shared_ptr<MacroConditionHttp> _entryData;
	bool _loading = true;
};

} // namespace advss
