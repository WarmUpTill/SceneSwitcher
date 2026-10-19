#include "macro-action-variable-json.hpp"
#include "json-helpers.hpp"

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{20,
	 {
		 {Action::EXTRACT_JSON,
		  "AdvSceneSwitcher.action.variable.type.extractJsonField",
		  "AdvSceneSwitcher.action.variable.layout.strValue", true},
		 {Action::QUERY_JSON,
		  "AdvSceneSwitcher.action.variable.type.queryJson",
		  "AdvSceneSwitcher.action.variable.layout.queryJson", true},
		 {Action::ARRAY_JSON,
		  "AdvSceneSwitcher.action.variable.type.accessJsonArray",
		  "AdvSceneSwitcher.action.variable.layout.accessJsonArray"},
	 },
	 []() { return std::make_unique<VariableActionJson>(); },
	 VariableActionJsonEdit::Create});

std::unique_ptr<VariableActionGroup> VariableActionJson::Copy() const
{
	return std::make_unique<VariableActionJson>(*this);
}

void VariableActionJson::Save(obs_data_t *obj,
			      const MacroActionVariable &) const
{
	_jsonQuery.Save(obj, "jsonQuery");
	_jsonIndex.Save(obj, "jsonIndex");
	obs_data_set_bool(obj, "jsonQueryExtractSingle",
			  _jsonQueryExtractSingle);
}

void VariableActionJson::Load(obs_data_t *obj, MacroActionVariable &)
{
	_jsonQuery.Load(obj, "jsonQuery");
	_jsonIndex.Load(obj, "jsonIndex");
	_jsonQueryExtractSingle =
		obs_data_get_bool(obj, "jsonQueryExtractSingle");
}

void VariableActionJson::ResolveVariables()
{
	_jsonQuery.ResolveVariables();
	_jsonIndex.ResolveVariables();
}

bool VariableActionJson::Perform(MacroActionVariable &action,
				 MacroActionVariable::Action type,
				 Variable &var)
{
	switch (type) {
	case Action::EXTRACT_JSON: {
		auto value = GetJsonField(var.Value(), action._strValue);
		if (!value.has_value()) {
			return true;
		}
		var.SetValue(*value);
		return true;
	}
	case Action::QUERY_JSON: {
		auto value = QueryJson(var.Value(), _jsonQuery);
		if (!value.has_value()) {
			return true;
		}

		if (!_jsonQueryExtractSingle) {
			var.SetValue(*value);
			return true;
		}

		auto extracted = ExtractSingleJsonArrayElement(*value);
		if (extracted.has_value()) {
			var.SetValue(*extracted);
		} else {
			var.SetValue(*value);
		}
		return true;
	}
	case Action::ARRAY_JSON: {
		auto value = AccessJsonArrayIndex(var.Value(), _jsonIndex);
		if (!value.has_value()) {
			return true;
		}
		var.SetValue(*value);
		return true;
	}
	default:
		return true;
	}
}

VariableActionJsonEdit::VariableActionJsonEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData),
	  _data(entryData ? entryData->GetGroup<VariableActionJson>()
			  : nullptr),
	  _jsonQuery(new VariableLineEdit(this)),
	  _jsonQueryHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.type.queryJson.info"),
		  this)),
	  _jsonIndex(new VariableSpinBox(this)),
	  _jsonExtractSingle(new QCheckBox(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.type.queryJson.extractSingle"),
		  this)),
	  _jsonExtractSingleHelp(new HelpIcon(
		  obs_module_text(
			  "AdvSceneSwitcher.action.variable.type.queryJson.extractSingle.info"),
		  this))
{
	_jsonIndex->setMaximum(999);

	QWidget::connect(_jsonQuery, SIGNAL(editingFinished()), this,
			 SLOT(JsonQueryChanged()));
	QWidget::connect(
		_jsonIndex,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(JsonIndexChanged(const NumberVariable<int> &)));
	QWidget::connect(_jsonExtractSingle, SIGNAL(stateChanged(int)), this,
			 SLOT(JsonQueryExtractSingleChanged(int)));

	auto layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_jsonExtractSingle);
	layout->addWidget(_jsonExtractSingleHelp);
	layout->addStretch();
	setLayout(layout);
}

VariableActionGroupEdit *
VariableActionJsonEdit::Create(QWidget *parent,
			       std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionJsonEdit(parent, entryData);
}

std::unordered_map<std::string, QWidget *>
VariableActionJsonEdit::GetInlineWidgets()
{
	return {
		{"{{jsonQuery}}", _jsonQuery},
		{"{{jsonQueryHelp}}", _jsonQueryHelp},
		{"{{jsonIndex}}", _jsonIndex},
	};
}

void VariableActionJsonEdit::UpdateEntryData()
{
	if (!_data) {
		return;
	}

	_jsonQuery->setText(_data->_jsonQuery);
	_jsonIndex->SetValue(_data->_jsonIndex);
	_jsonExtractSingle->setChecked(_data->_jsonQueryExtractSingle);
	_loading = false;
}

void VariableActionJsonEdit::SetWidgetVisibility(
	MacroActionVariable::Action action)
{
	setVisible(action == Action::QUERY_JSON);
}

void VariableActionJsonEdit::JsonQueryChanged()
{
	GUARD_LOADING_AND_LOCK();
	_data->_jsonQuery = _jsonQuery->text().toStdString();
}

void VariableActionJsonEdit::JsonIndexChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_jsonIndex = value;
}

void VariableActionJsonEdit::JsonQueryExtractSingleChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_data->_jsonQueryExtractSingle = value;
}

} // namespace advss
