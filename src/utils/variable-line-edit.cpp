#include "variable-line-edit.hpp"

VariableLineEdit::VariableLineEdit(QWidget *parent) : QLineEdit(parent) {}

void VariableLineEdit::setText(const QString &string)
{
	QLineEdit::setText(string);
}

void VariableLineEdit::setText(const VariableResolvingString &string)
{
	QLineEdit::setText(QString::fromStdString(string.UnresolvedValue()));
}
