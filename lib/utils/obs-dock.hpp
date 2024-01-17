#pragma once
#include <QDockWidget>

// QDockWidget wrapper enable applying "OBSDock" stylesheet
class OBSDock : public QDockWidget {
	Q_OBJECT

public:
	inline OBSDock(QWidget *parent = nullptr) : QDockWidget(parent) {}
};
