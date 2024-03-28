#include "single-char-selection.hpp"

namespace advss {

SingleCharSelection::SingleCharSelection(QWidget *parent) : QLineEdit(parent)
{
	setMaxLength(1);
	setMaximumWidth(50);
	connect(this, &QLineEdit::textChanged, this,
		&SingleCharSelection::CharChanged);
}

void SingleCharSelection::HandleTextChanged(const QString &text)
{
	if (text.length() == 1) {
		emit CharChanged(text);
	} else if (text.length() > 1) {
		setText(text.left(1));
	}
}

} // namespace advss
