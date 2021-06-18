#include <QVBoxLayout>
#include <QDialogButtonBox>
#include "headers/name-dialog.hpp"

AdvSSNameDialog::AdvSSNameDialog(QWidget *parent) : QDialog(parent)
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);
	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	label = new QLabel(this);
	layout->addWidget(label);
	label->setText("Set Text");

	userText = new QLineEdit(this);
	layout->addWidget(userText);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonbox);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

static bool IsWhitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

static void CleanWhitespace(std::string &str)
{
	while (str.size() && IsWhitespace(str.back()))
		str.erase(str.end() - 1);
	while (str.size() && IsWhitespace(str.front()))
		str.erase(str.begin());
}

bool AdvSSNameDialog::AskForName(QWidget *parent, const QString &title,
				 const QString &text,
				 std::string &userTextInput,
				 const QString &placeHolder, int maxSize,
				 bool clean)
{
	if (maxSize <= 0 || maxSize > 32767) {
		maxSize = 170;
	}

	AdvSSNameDialog dialog(parent);
	dialog.setWindowTitle(title);

	dialog.label->setText(text);
	dialog.userText->setMaxLength(maxSize);
	dialog.userText->setText(placeHolder);
	dialog.userText->selectAll();

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	userTextInput = dialog.userText->text().toUtf8().constData();
	if (clean) {
		CleanWhitespace(userTextInput);
	}
	return true;
}
