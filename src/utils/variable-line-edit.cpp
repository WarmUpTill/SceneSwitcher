#include "variable-line-edit.hpp"

#include <obs-module.h>

namespace advss {

VariableLineEdit::VariableLineEdit(QWidget *parent) : QLineEdit(parent)
{
	QLineEdit::setToolTip(
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));
}

void VariableLineEdit::setText(const QString &string)
{
	QLineEdit::setText(string);
}

void VariableLineEdit::setText(const StringVariable &string)
{
	QLineEdit::setText(QString::fromStdString(string.UnresolvedValue()));
}

void VariableLineEdit::setToolTip(const QString &string)
{
	QLineEdit::setToolTip(
		string + "\n" +
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));
}

} // namespace advss
