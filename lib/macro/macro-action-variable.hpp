#pragma once
#include "macro-action-edit.hpp"
#include "variable-ref.hpp"
#include "variable-text-edit.hpp"
#include "variable.hpp"

#include <memory>
#include <vector>

namespace advss {

class VariableActionGroup;
class VariableActionGroupEdit;

class MacroActionVariable : public MacroAction {
public:
	MacroActionVariable(Macro *m);
	MacroActionVariable(const MacroActionVariable &);
	~MacroActionVariable();
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	bool PostLoad();
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	std::vector<TempVariableRef> GetTempVarRefs() const;
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	template<class T> T *GetGroup() const
	{
		for (const auto &group : _groups) {
			if (auto result = dynamic_cast<T *>(group.get())) {
				return result;
			}
		}
		return nullptr;
	}

	// Values are persisted, do not change existing ones
	enum class Action {
		SET_VALUE,
		APPEND,
		APPEND_VAR,
		INCREMENT,
		DECREMENT,
		SET_CONDITION_VALUE,
		SET_ACTION_VALUE,
		ROUND_TO_INT,
		SUBSTRING,
		FIND_AND_REPLACE,
		MATH_EXPRESSION,
		USER_INPUT,
		ENV_VARIABLE,
		SCENE_ITEM_COUNT,
		STRING_LENGTH,
		EXTRACT_JSON,
		SET_TO_TEMPVAR,
		SCENE_ITEM_NAME,
		PAD,
		TRUNCATE,
		SWAP_VALUES,
		TRIM,
		CHANGE_CASE,
		RANDOM_NUMBER,
		QUERY_JSON,
		ARRAY_JSON,
		COPY_VAR,
		RANDOM_LIST_VALUE,
	};

	Action _action = Action::SET_VALUE;
	VariableRef _variable;
	VariableRef _variable2;
	StringVariable _strValue = "";

private:
	std::vector<std::unique_ptr<VariableActionGroup>> _groups;

	static bool _registered;
	static const std::string id;
};

class MacroActionVariableEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionVariableEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionVariable> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionVariableEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionVariable>(action));
	}

private slots:
	void VariableChanged(const QString &);
	void Variable2Changed(const QString &);
	void ActionChanged(int);
	void StrValueChanged();

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() const;

	VariableSelection *_variables;
	VariableSelection *_variables2;
	FilterComboBox *_actions;
	VariableTextEdit *_strValue;
	QHBoxLayout *_entryLayout;
	std::vector<VariableActionGroupEdit *> _groupEdits;

	std::shared_ptr<MacroActionVariable> _entryData;
	bool _loading = true;
};

} // namespace advss
