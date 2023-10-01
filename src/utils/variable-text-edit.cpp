#include "variable-text-edit.hpp"
#include "switcher-data.hpp"

#include <obs-module.h>

namespace advss {

VariableTextEdit::VariableTextEdit(QWidget *parent)
	: ResizingPlainTextEdit(parent)
{
	QPlainTextEdit::setToolTip(
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));
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

void VariableTextEdit::setToolTip(const QString &string)
{
	QPlainTextEdit::setToolTip(
		string + "\n" +
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));
}

} // namespace advss
