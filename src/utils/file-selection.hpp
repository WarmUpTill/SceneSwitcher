#pragma once
#include "variable-line-edit.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace advss {

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
	void SetPath(const StringVariable &);
	void SetPath(const QString &);
	QPushButton *Button() { return _browseButton; }
	static QString ValidPathOrDesktop(const QString &path);

private slots:
	void BrowseButtonClicked();
	void PathChange();
signals:
	void PathChanged(const QString &);

private:
	Type _type;
	VariableLineEdit *_filePath;
	QPushButton *_browseButton;
};

} // namespace advss
