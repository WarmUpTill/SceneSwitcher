#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>

// Based on OBS's NameDialog
class AdvSSNameDialog : public QDialog {
	Q_OBJECT

public:
	AdvSSNameDialog(QWidget *parent);

	// Returns true if user clicks OK, false otherwise
	// userTextInput returns string that user typed into dialog
	static bool AskForName(QWidget *parent, const QString &title,
			       const QString &text, std::string &userTextInput,
			       const QString &placeHolder = QString(""),
			       int maxSize = 170, bool clean = true);

private:
	QLabel *label;
	QLineEdit *userText;
};
