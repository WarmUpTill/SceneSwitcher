#pragma once
#include <QPlainTextEdit>

namespace advss {

class ResizingPlainTextEdit : public QPlainTextEdit {
	Q_OBJECT
public:
	ResizingPlainTextEdit(QWidget *parent, const int scrollAt = 10,
			      const int minLines = 3,
			      const int paddingLines = 2);
	virtual ~ResizingPlainTextEdit(){};
	int maxLength();
	void setMaxLength(int maxLength);

private slots:
	void ResizeTexteditArea();
	void PreventExceedingMaxLength();

private:
	const int _scrollAt;
	const int _minLines;
	const int _paddingLines;
	int _maxLength = -1;
};

} // namespace advss
