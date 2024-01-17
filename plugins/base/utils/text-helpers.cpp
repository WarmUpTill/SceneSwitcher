#include "text-helpers.hpp"

#include <regex>

namespace advss {

QString EscapeForRegex(const QString &s)
{
	static std::regex specialChars{R"([-[\]{}()*+?.,\^$|#\s])"};
	std::string input = s.toStdString();
	return QString::fromStdString(
		std::regex_replace(input, specialChars, R"(\$&)"));
}

} // namespace advss
