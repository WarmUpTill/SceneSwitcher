#include "variable-text-edit.hpp"
#include "switcher-data-structs.hpp"

VariableTextEdit::VariableTextEdit(QWidget *parent)
	: ResizingPlainTextEdit(parent)
{
}

void VariableTextEdit::setPlainText(const QString &string)
{
	QPlainTextEdit::setPlainText(string);
}

void VariableTextEdit::setPlainText(const VariableResolvingString &string)
{
	QPlainTextEdit::setPlainText(
		QString::fromStdString(string.UnresolvedValue()));
}
