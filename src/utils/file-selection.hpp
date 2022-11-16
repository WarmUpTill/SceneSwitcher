#pragma once
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

class FileSelection : public QWidget {
	Q_OBJECT

public:
	enum class Type {
		READ,
		WRITE,
		FOLDER,
	};

	FileSelection(FileSelection::Type type = FileSelection::Type::READ,
		      QWidget *parent = 0);
	void SetPath(const QString &);
	QPushButton *Button() { return _browseButton; }

private slots:
	void BrowseButtonClicked();
	void PathChange();
signals:
	void PathChanged(const QString &);

private:
	Type _type;
	QLineEdit *_filePath;
	QPushButton *_browseButton;
};
