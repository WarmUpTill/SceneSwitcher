#pragma once
#include "export-symbol-helper.hpp"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>

namespace advss {

// Based on OBS's NameDialog
class NameDialog : public QDialog {
	Q_OBJECT

public:
	NameDialog(QWidget *parent);

	// Returns true if user clicks OK, false otherwise
	// userTextInput returns string that user typed into dialog
	EXPORT static bool AskForName(QWidget *parent, const QString &title,
				      const QString &prompt,
				      std::string &userTextInput,
				      const QString &placeHolder = QString(""),
				      int maxSize = 170, bool clean = true);

private:
	QLabel *_label;
	QLineEdit *_userText;
};

} // namespace advss
