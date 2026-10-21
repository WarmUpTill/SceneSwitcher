#pragma once
#include <QAbstractItemModel>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QStringList>

#include <string>
#include <vector>

class QWidget;

namespace advss::test {

void ProcessEvents();

// Mouse input is sent to the native window so that Qt performs the same hit
// testing, implicit grabbing and drag and drop handling as for real input
void MousePress(QWidget *widget, const QPoint &pos,
		Qt::KeyboardModifiers modifiers = Qt::NoModifier);
void MouseRelease(QWidget *widget, const QPoint &pos,
		  Qt::KeyboardModifiers modifiers = Qt::NoModifier);
void MouseMove(QWidget *widget, const QPoint &pos,
	       Qt::MouseButtons buttons = Qt::NoButton,
	       Qt::KeyboardModifiers modifiers = Qt::NoModifier);
void Click(QWidget *widget, const QPoint &pos,
	   Qt::KeyboardModifiers modifiers = Qt::NoModifier);
void DoubleClick(QWidget *widget, const QPoint &pos);
bool DragAndDropSupported();
void DragAndDrop(QWidget *widget, const QPoint &from, const QPoint &to);

// Validates the rows*() signals and the parent/child structure of a model and
// records every inconsistency it detects
class ModelChecker : public QObject {
	Q_OBJECT

public:
	ModelChecker(QAbstractItemModel *model, int nameRole);
	const std::vector<std::string> &Errors() const { return _errors; }
	void CheckStructure();

private slots:
	void RowsAboutToBeInserted(const QModelIndex &parent, int first,
				   int last);
	void RowsInserted(const QModelIndex &parent, int first, int last);
	void RowsAboutToBeRemoved(const QModelIndex &parent, int first,
				  int last);
	void RowsRemoved(const QModelIndex &parent, int first, int last);
	void RowsAboutToBeMoved(const QModelIndex &sourceParent,
				int sourceStart, int sourceEnd,
				const QModelIndex &destinationParent,
				int destinationRow);
	void RowsMoved(const QModelIndex &sourceParent, int sourceStart,
		       int sourceEnd, const QModelIndex &destinationParent,
		       int destinationRow);

private:
	void CheckStructure(const QModelIndex &parent, int depth);
	QString Name(const QModelIndex &index) const;
	void Error(const std::string &message);

	QAbstractItemModel *_model;
	int _nameRole;

	struct Change {
		QPersistentModelIndex parent;
		int oldCount = 0;
		QPersistentModelIndex destinationParent;
		int oldDestinationCount = 0;
		QStringList names;
	};
	Change _pending;
	std::vector<std::string> _errors;
};

} // namespace advss::test
