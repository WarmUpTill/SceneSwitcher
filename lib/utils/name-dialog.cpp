#include "name-dialog.hpp"

#include <QVBoxLayout>
#include <QDialogButtonBox>

namespace advss {

NameDialog::NameDialog(QWidget *parent)
	: QDialog(parent),
	  _label(new QLabel(this)),
	  _userText(new QLineEdit(this))

{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);
	auto layout = new QVBoxLayout;
	setLayout(layout);

	layout->addWidget(_label);
	layout->addWidget(_userText);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonbox);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

static bool isWhitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

static void cleanWhitespace(std::string &str)
{
	while (str.size() && isWhitespace(str.back())) {
		str.erase(str.end() - 1);
	}
	while (str.size() && isWhitespace(str.front())) {
		str.erase(str.begin());
	}
}

bool NameDialog::AskForName(QWidget *parent, const QString &title,
			    const QString &prompt, std::string &userTextInput,
			    const QString &placeHolder, int maxSize, bool clean)
{
	if (maxSize <= 0 || maxSize > 32767) {
		maxSize = 170;
	}

	NameDialog dialog(parent);
	dialog.setWindowTitle(title);

	dialog._label->setVisible(!prompt.isEmpty());
	dialog._label->setText(prompt);
	dialog._userText->setMaxLength(maxSize);
	dialog._userText->setText(placeHolder);
	dialog._userText->selectAll();

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	userTextInput = dialog._userText->text().toUtf8().constData();
	if (clean) {
		cleanWhitespace(userTextInput);
	}
	return true;
}

} // namespace advss
