#pragma once
#include "export-symbol-helper.hpp"
#include "list-controls.hpp"

#include <QLabel>
#include <QListWidget>
#include <QLayout>

namespace advss {

class ADVSS_EXPORT ListEditor : public QWidget {
	Q_OBJECT

public:
	ListEditor(QWidget *parent = nullptr, bool reorder = true);
	int count() const { return _list->count(); };
	void SetPlaceholderText(const QString &text);
	void SetMinListHeight(int);
	void SetMaxListHeight(int);

protected:
	void showEvent(QShowEvent *);
	bool eventFilter(QObject *, QEvent *);

private slots:
	virtual void Add() = 0;
	virtual void Remove() = 0;
	virtual void Up(){};
	virtual void Down(){};
	virtual void Clicked(QListWidgetItem *) {}
	void UpdatePlaceholder();

protected:
	void UpdateListSize();
	int GetIndexOfSignal() const;

	QListWidget *_list;
	ListControls *_controls;
	QVBoxLayout *_mainLayout;

private:
	QLabel *_placeholder;
	int _minHeight = -1;
	int _maxHeight = -1;
};

} // namespace advss
