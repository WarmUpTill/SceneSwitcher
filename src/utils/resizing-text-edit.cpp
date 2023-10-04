#include "resizing-text-edit.hpp"

#include <obs-module.h>
#include <utility.hpp>

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
	QWidget::connect(this, SIGNAL(textChanged()), this,
			 SLOT(PreventExceedingMaxLength()));
}

int ResizingPlainTextEdit::maxLength()
{
	return _maxLength;
}

void ResizingPlainTextEdit::setMaxLength(int maxLength)
{
	_maxLength = maxLength;
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

void ResizingPlainTextEdit::PreventExceedingMaxLength()
{
	auto plainText = QPlainTextEdit::toPlainText();

	if (_maxLength > -1 && plainText.length() > _maxLength) {
		QPlainTextEdit::setPlainText(plainText.left(_maxLength));
		QPlainTextEdit::moveCursor(QTextCursor::End);

		DisplayMessage(
			QString(obs_module_text(
					"AdvSceneSwitcher.validation.text.maxLength"))
				.arg(QString::number(_maxLength)));
	}
}

} // namespace advss
