#include "resizing-text-edit.hpp"

namespace advss {

ResizingPlainTextEdit::ResizingPlainTextEdit(QWidget *parent,
					     const int scrollAt,
					     const int minLines,
					     const int paddingLines)
	: QPlainTextEdit(parent),
	  _scrollAt(scrollAt),
	  _minLines(minLines),
	  _paddingLines(paddingLines)
{
	QWidget::connect(this, SIGNAL(textChanged()), this,
			 SLOT(ResizeTexteditArea()));
}

void ResizingPlainTextEdit::ResizeTexteditArea()
{
	QFontMetrics f(font());
	int rowHeight = f.lineSpacing();
	int numLines = document()->blockCount();
	if (numLines + _paddingLines < _minLines) {
		setFixedHeight(_minLines * rowHeight);
	} else if (numLines + _paddingLines < _scrollAt) {
		setFixedHeight((numLines + _paddingLines) * rowHeight);
	} else {
		setFixedHeight(_scrollAt * rowHeight);
	}

	adjustSize();
	updateGeometry();
}

} // namespace advss
