#include "file-selection.hpp"
#include "obs-module-helper.hpp"

#include <QLayout>
#include <QFileDialog>
#include <QStandardPaths>

namespace advss {

FileSelection::FileSelection(FileSelection::Type type, QWidget *parent)
	: QWidget(parent),
	  _type(type),
	  _filePath(new VariableLineEdit(this)),
	  _browseButton(
		  new QPushButton(obs_module_text("AdvSceneSwitcher.browse")))
{
	QWidget::connect(_filePath, SIGNAL(editingFinished()), this,
			 SLOT(PathChange()));
	QWidget::connect(_browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));
	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(_filePath);
	layout->addWidget(_browseButton);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
}

void FileSelection::SetPath(const StringVariable &path)
{
	_filePath->setText(path);
}

void FileSelection::SetPath(const QString &path)
{
	_filePath->setText(path);
}

QString FileSelection::ValidPathOrDesktop(const QString &path)
{
	QFileInfo fileInfo(path);
	if (fileInfo.isFile()) {
		return path;
	}
	return QStandardPaths::writableLocation(
		QStandardPaths::DesktopLocation);
}

void FileSelection::BrowseButtonClicked()
{
	QString defaultPath = ValidPathOrDesktop(_filePath->text());
	QString path;
	if (_type == FileSelection::Type::WRITE) {
		path = QFileDialog::getSaveFileName(this, "", defaultPath);
	} else if (_type == FileSelection::Type::READ) {
		path = QFileDialog::getOpenFileName(this, "", defaultPath);
	} else {
		path = QFileDialog::getExistingDirectory(this, "", defaultPath);
	}

	if (path.isEmpty()) {
		return;
	}

	_filePath->setText(path);
	emit PathChanged(path);
}

void FileSelection::PathChange()
{
	emit PathChanged(_filePath->text());
}

} // namespace advss
