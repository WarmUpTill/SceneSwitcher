#include "file-selection.hpp"

#include <obs-module.h>
#include <QLayout>
#include <QFileDialog>

FileSelection::FileSelection(FileSelection::Type type, QWidget *parent)
	: QWidget(parent), _type(type)
{
	_filePath = new QLineEdit();
	_browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));

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

void FileSelection::SetPath(const QString &path)
{
	_filePath->setText(path);
}

void FileSelection::BrowseButtonClicked()
{
	QString path;
	if (_type == FileSelection::Type::WRITE) {
		path = QFileDialog::getSaveFileName(this);
	} else {
		path = QFileDialog::getOpenFileName(this);
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
