#include "variable-line-edit.hpp"

namespace advss {

VariableLineEdit::VariableLineEdit(QWidget *parent) : QLineEdit(parent) {}

void VariableLineEdit::setText(const QString &string)
{
	QLineEdit::setText(string);
}

void VariableLineEdit::setText(const StringVariable &string)
{
	QLineEdit::setText(QString::fromStdString(string.UnresolvedValue()));
}

} // namespace advss
