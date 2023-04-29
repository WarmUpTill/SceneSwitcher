#include "variable-text-edit.hpp"
#include "switcher-data.hpp"

namespace advss {

VariableTextEdit::VariableTextEdit(QWidget *parent)
	: ResizingPlainTextEdit(parent)
{
}

void VariableTextEdit::setPlainText(const QString &string)
{
	QPlainTextEdit::setPlainText(string);
}

void VariableTextEdit::setPlainText(const StringVariable &string)
{
	QPlainTextEdit::setPlainText(
		QString::fromStdString(string.UnresolvedValue()));
}

} // namespace advss
