#include "text-helpers.hpp"

#include <QRegularExpression>

namespace advss {

QString EscapeForRegex(const QString &input)
{
	return QRegularExpression::escape(input);
}

} // namespace advss
