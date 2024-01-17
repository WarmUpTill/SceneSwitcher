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

	EXPORT
	FileSelection(FileSelection::Type type = FileSelection::Type::READ,
		      QWidget *parent = 0);
	EXPORT void SetPath(const StringVariable &);
	EXPORT void SetPath(const QString &);
	EXPORT QPushButton *Button() { return _browseButton; }
	EXPORT static QString ValidPathOrDesktop(const QString &path);

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
