#include "window-selection.hpp"
#include "selection-helpers.hpp"

namespace advss {

WindowSelectionWidget::WindowSelectionWidget(QWidget *parent)
	: FilterComboBox(parent)
{
	setEditable(true);
	SetAllowUnmatchedSelection(true);
	setMaxVisibleItems(20);
	PopulateWindowSelection(this);
}

void WindowSelectionWidget::showEvent(QShowEvent *event)
{
	FilterComboBox::showEvent(event);
	const QSignalBlocker b(this);
	const auto text = currentText();
	clear();
	PopulateWindowSelection(this);
	setCurrentText(text);
}

} // namespace advss
