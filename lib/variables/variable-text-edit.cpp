#include "variable-text-edit.hpp"
#include "obs-module-helper.hpp"

namespace advss {

VariableTextEdit::VariableTextEdit(QWidget *parent, const int scrollAt,
				   const int minLines, const int paddingLines)
	: ResizingPlainTextEdit(parent, scrollAt, minLines, paddingLines)
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
