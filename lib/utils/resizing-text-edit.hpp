#pragma once
#include "export-symbol-helper.hpp"

#include <QPlainTextEdit>

namespace advss {

class ResizingPlainTextEdit : public QPlainTextEdit {
	Q_OBJECT
public:
	ResizingPlainTextEdit(QWidget *parent, const int scrollAt = 10,
			      const int minLines = 3,
			      const int paddingLines = 2);
	virtual ~ResizingPlainTextEdit(){};
	EXPORT int maxLength();
	EXPORT void setMaxLength(int maxLength);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
	void ResizeTexteditArea();
	void PreventExceedingMaxLength();

private:
	void AddHeightForScrollBar(bool addHeight);

	const int _scrollAt;
	const int _minLines;
	const int _paddingLines;
	int _maxLength = -1;
	int _hScrollBarAddedHeight = 0;
};

} // namespace advss
