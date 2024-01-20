#include "resizing-text-edit.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QScrollBar>

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
	setWordWrapMode(QTextOption::NoWrap);
	QWidget::connect(this, SIGNAL(textChanged()), this,
			 SLOT(ResizeTexteditArea()));
	QWidget::connect(this, SIGNAL(textChanged()), this,
			 SLOT(PreventExceedingMaxLength()));
	verticalScrollBar()->installEventFilter(this);
}

int ResizingPlainTextEdit::maxLength()
{
	return _maxLength;
}

void ResizingPlainTextEdit::setMaxLength(int maxLength)
{
	_maxLength = maxLength;
}

bool ResizingPlainTextEdit::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == horizontalScrollBar()) {
		if (event->type() == QEvent::Show) {
			AddHeightForScrollBar(true);
		} else if (event->type() == QEvent::Hide) {
			AddHeightForScrollBar(false);
		}
	}

	return QPlainTextEdit::eventFilter(obj, event);
}

void ResizingPlainTextEdit::ResizeTexteditArea()
{
	QFontMetrics f(font());
	int rowHeight = f.lineSpacing();
	int numLines = document()->blockCount();

	int height = 0;
	if (numLines + _paddingLines < _minLines) {
		height = _minLines * rowHeight;
	} else if (numLines + _paddingLines < _scrollAt) {
		height = (numLines + _paddingLines) * rowHeight;
	} else {
		height = _scrollAt * rowHeight;
	}

	setFixedHeight(height + _hScrollBarAddedHeight);
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

void ResizingPlainTextEdit::AddHeightForScrollBar(bool addHeight)
{
	if (!addHeight) {
		setFixedHeight(height() - _hScrollBarAddedHeight);
		_hScrollBarAddedHeight = 0;
		return;
	}

	// Check if we have already added space for the scroll bar
	if (_hScrollBarAddedHeight > 0) {
		return;
	}

	_hScrollBarAddedHeight = verticalScrollBar()->height();
	setFixedHeight(height() + _hScrollBarAddedHeight);
}

} // namespace advss
