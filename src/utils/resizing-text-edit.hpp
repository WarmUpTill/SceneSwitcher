#pragma once
#include <QPlainTextEdit>

class ResizingPlainTextEdit : public QPlainTextEdit {
	Q_OBJECT
public:
	ResizingPlainTextEdit(QWidget *parent, const int scrollAt = 10,
			      const int minLines = 3,
			      const int paddingLines = 2);
	virtual ~ResizingPlainTextEdit(){};
private slots:
	void ResizeTexteditArea();

private:
	const int _scrollAt;
	const int _minLines;
	const int _paddingLines;
};
