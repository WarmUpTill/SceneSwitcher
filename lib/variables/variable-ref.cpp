#include "variable-ref.hpp"
#include "variable.hpp"

namespace advss {

thread_local static VariableRefCollector *activeCollector = nullptr;

VariableRefCollector::VariableRefCollector() : _previous(activeCollector)
{
	activeCollector = this;
}

VariableRefCollector::~VariableRefCollector()
{
	activeCollector = _previous;
}

void VariableRefCollector::Add(const std::string &name, int count)
{
	_counts[name] += count;
}

void ReportVariableRef(const std::weak_ptr<Variable> &variable)
{
	if (!activeCollector) {
		return;
	}

	auto var = variable.lock();
	if (!var) {
		return;
	}

	activeCollector->Add(var->Name());
}

static int countOccurrences(const std::string &text, const std::string &pattern)
{
	int count = 0;
	for (auto pos = text.find(pattern); pos != std::string::npos;
	     pos = text.find(pattern, pos + pattern.length())) {
		count++;
	}
	return count;
}

void ReportVariableRefsInText(const std::string &text)
{
	if (!activeCollector || text.find("${") == std::string::npos) {
		return;
	}

	for (const auto &variable : GetVariables()) {
		const auto &name = variable->Name();
		const int count = countOccurrences(text, "${" + name + "}");
		if (count > 0) {
			activeCollector->Add(name, count);
		}
	}
}

void VariableRef::Save(obs_data_t *obj, const char *key) const
{
	auto var = _variable.lock();
	if (!var) {
		return;
	}

	obs_data_set_string(obj, key, var->Name().c_str());
	ReportVariableRef(_variable);
}

void VariableRef::Load(obs_data_t *obj, const char *key)
{
	_variable = GetWeakVariableByName(obs_data_get_string(obj, key));
}

} // namespace advss
