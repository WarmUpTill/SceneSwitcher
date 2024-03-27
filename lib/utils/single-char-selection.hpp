#pragma once
#include <QLineEdit>

namespace advss {

class SingleCharSelection : public QLineEdit {
	Q_OBJECT
public:
	SingleCharSelection(QWidget *parent = nullptr);

signals:
	void CharChanged(const QString &character);

private slots:
	void HandleTextChanged(const QString &text);
};

} // namespace advss
