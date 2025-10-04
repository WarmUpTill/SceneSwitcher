#pragma once
#include <QFrame>
#include <QMainWindow>
#include <QWidget>

namespace advss {

class MacroDockWindow : public QFrame {
	Q_OBJECT

public:
	MacroDockWindow();
	QWidget *AddMacroDock(QWidget *, const QString &title);
	void RemoveMacroDock(QWidget *);

private:
	QMainWindow *_window;
};

} // namespace advss
