#pragma once
#include "export-symbol-helper.hpp"
#include "list-controls.hpp"

#include <QListWidget>
#include <QLayout>

namespace advss {

class ADVSS_EXPORT ListEditor : public QWidget {
	Q_OBJECT

public:
	ListEditor(QWidget *parent = nullptr, bool reorder = true);

protected:
	void showEvent(QShowEvent *);

private slots:
	virtual void Add() = 0;
	virtual void Remove() = 0;
	virtual void Up(){};
	virtual void Down(){};
	virtual void Clicked(QListWidgetItem *) {}

protected:
	void UpdateListSize();
	int GetIndexOfSignal() const;

	QListWidget *_list;
	ListControls *_controls;
	QVBoxLayout *_mainLayout;
};

} // namespace advss
