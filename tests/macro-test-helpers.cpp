#include "macro-test-helpers.hpp"

#include <macro.hpp>
#include <macro-helpers.hpp>

#include <cctype>
#include <deque>
#include <vector>

namespace advss::test {

namespace {

class LayoutParser {
public:
	explicit LayoutParser(const std::string &layout) : _text(layout) {}

	std::deque<std::shared_ptr<Macro>> Parse()
	{
		std::deque<std::shared_ptr<Macro>> result;
		ParseList(result);
		return result;
	}

private:
	std::vector<std::shared_ptr<Macro>>
	ParseList(std::deque<std::shared_ptr<Macro>> &out)
	{
		std::vector<std::shared_ptr<Macro>> items;
		SkipSpaces();
		while (_pos < _text.size() && _text[_pos] != ']' &&
		       _text[_pos] != '}') {
			items.emplace_back(ParseItem(out));
			SkipSpaces();
			if (_pos < _text.size() && _text[_pos] == ',') {
				_pos++;
			}
			SkipSpaces();
		}
		return items;
	}

	std::shared_ptr<Macro>
	ParseItem(std::deque<std::shared_ptr<Macro>> &out)
	{
		std::string name;
		while (_pos < _text.size() &&
		       std::isalnum(static_cast<unsigned char>(_text[_pos]))) {
			name += _text[_pos++];
		}

		if (_pos >= _text.size() ||
		    (_text[_pos] != '[' && _text[_pos] != '{')) {
			auto macro = std::make_shared<Macro>(name);
			out.emplace_back(macro);
			return macro;
		}

		const bool collapsed = _text[_pos] == '{';
		_pos++;
		std::deque<std::shared_ptr<Macro>> descendants;
		auto children = ParseList(descendants);
		_pos++;

		auto group = Macro::CreateGroup(name, children);
		group->SetCollapsed(collapsed);
		out.emplace_back(group);
		out.insert(out.end(), descendants.begin(), descendants.end());
		return group;
	}

	void SkipSpaces()
	{
		while (_pos < _text.size() && _text[_pos] == ' ') {
			_pos++;
		}
	}

	std::string _text;
	size_t _pos = 0;
};

std::string describeChildren(const Macro *parent)
{
	std::string result;
	for (const auto &macro : GetTopLevelMacros()) {
		if (macro->Parent().get() != parent) {
			continue;
		}
		if (!result.empty()) {
			result += ", ";
		}
		result += macro->Name();
		if (!macro->IsGroup()) {
			continue;
		}
		const bool collapsed = macro->IsCollapsed();
		result += collapsed ? "{" : "[";
		result += describeChildren(macro.get());
		result += collapsed ? "}" : "]";
	}
	return result;
}

} // namespace

void SetupMacros(const std::string &layout)
{
	GetTopLevelMacros() = LayoutParser(layout).Parse();
}

std::string DescribeMacros()
{
	return describeChildren(nullptr);
}

std::shared_ptr<Macro> GetMacro(const std::string &name)
{
	return GetWeakMacroByName(name.c_str()).lock();
}

} // namespace advss::test
