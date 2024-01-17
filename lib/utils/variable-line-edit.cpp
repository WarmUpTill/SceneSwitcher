#include "variable-line-edit.hpp"
#include "obs-module-helper.hpp"

#include <utility.hpp>

namespace advss {

VariableLineEdit::VariableLineEdit(QWidget *parent) : QLineEdit(parent)
{
	QLineEdit::setToolTip(
		obs_module_text("AdvSceneSwitcher.tooltip.availableVariables"));

	QWidget::connect(this, SIGNAL(inputRejected()), this,
			 SLOT(DisplayValidationMessages()));
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

void VariableLineEdit::DisplayValidationMessages()
{
	int maxLength = QLineEdit::maxLength();

	if (QLineEdit::text().length() == maxLength) {
		DisplayMessage(
			QString(obs_module_text(
					"AdvSceneSwitcher.validation.text.maxLength"))
				.arg(QString::number(maxLength)));
	}
}

} // namespace advss
