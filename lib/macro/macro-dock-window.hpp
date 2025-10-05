#pragma once
#include <QFrame>
#include <QMainWindow>
#include <QWidget>

#include <string>
#include <vector>

namespace advss {

class MacroDockWindow : public QFrame {
	Q_OBJECT

public:
	MacroDockWindow(const std::string &name);
	QWidget *AddMacroDock(QWidget *, const QString &title);
	void RenameMacro(const std::string &oldName,
			 const std::string &newName);
	void RemoveMacroDock(QWidget *);
	QMainWindow *GetWindow() const;

private:
	std::string _name;
	QMainWindow *_window;
};

MacroDockWindow *GetDockWindowByName(const std::string &name);

} // namespace advss
