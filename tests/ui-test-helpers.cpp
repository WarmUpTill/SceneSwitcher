#include "ui-test-helpers.hpp"
#include "catch.hpp"

#include <QApplication>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>

namespace advss::test {

void ProcessEvents()
{
	for (int i = 0; i < 3; i++) {
		QCoreApplication::processEvents();
		QCoreApplication::sendPostedEvents(nullptr,
						   QEvent::DeferredDelete);
	}
}

static void sendMouseEvent(QWidget *widget, QEvent::Type type,
			   const QPoint &pos, Qt::MouseButton button,
			   Qt::MouseButtons buttons,
			   Qt::KeyboardModifiers modifiers)
{
	auto topLevel = widget->window();
	auto window = topLevel->windowHandle();
	Q_ASSERT(window);
	const QPointF windowPos = widget->mapTo(topLevel, pos);
	const QPointF globalPos = widget->mapToGlobal(pos);
	QMouseEvent event(type, windowPos, windowPos, globalPos, button,
			  buttons, modifiers);
	QApplication::sendEvent(window, &event);
}

void MousePress(QWidget *widget, const QPoint &pos,
		Qt::KeyboardModifiers modifiers)
{
	sendMouseEvent(widget, QEvent::MouseButtonPress, pos, Qt::LeftButton,
		       Qt::LeftButton, modifiers);
}

void MouseRelease(QWidget *widget, const QPoint &pos,
		  Qt::KeyboardModifiers modifiers)
{
	sendMouseEvent(widget, QEvent::MouseButtonRelease, pos, Qt::LeftButton,
		       Qt::NoButton, modifiers);
}

void MouseMove(QWidget *widget, const QPoint &pos, Qt::MouseButtons buttons,
	       Qt::KeyboardModifiers modifiers)
{
	sendMouseEvent(widget, QEvent::MouseMove, pos, Qt::NoButton, buttons,
		       modifiers);
}

void Click(QWidget *widget, const QPoint &pos, Qt::KeyboardModifiers modifiers)
{
	MousePress(widget, pos, modifiers);
	MouseRelease(widget, pos, modifiers);
	ProcessEvents();
}

void DoubleClick(QWidget *widget, const QPoint &pos)
{
	MousePress(widget, pos);
	MouseRelease(widget, pos);
	sendMouseEvent(widget, QEvent::MouseButtonDblClick, pos, Qt::LeftButton,
		       Qt::LeftButton, Qt::NoModifier);
	MouseRelease(widget, pos);
	ProcessEvents();
}

bool DragAndDropSupported()
{
	// QOffscreenDrag::drag() ignores all drags
	return QGuiApplication::platformName() != "offscreen";
}

void DragAndDrop(QWidget *widget, const QPoint &from, const QPoint &to)
{
	if (!DragAndDropSupported()) {
		FAIL("drag and drop is not supported by the Qt platform");
	}

	MousePress(widget, from);
	// The first move puts the view into dragging state, the second one
	// starts the drag. QDrag::exec() then runs its own event loop, so the
	// rest of the gesture has to be delivered from within that loop.
	MouseMove(widget, from + QPoint(0, 1), Qt::LeftButton);
	QTimer::singleShot(0, widget, [widget, to]() {
		MouseMove(widget, to, Qt::LeftButton);
		MouseMove(widget, to, Qt::LeftButton);
		MouseRelease(widget, to);
	});
	const int distance = QApplication::startDragDistance() + 5;
	MouseMove(widget, from + QPoint(0, distance), Qt::LeftButton);
	ProcessEvents();
}

ModelChecker::ModelChecker(QAbstractItemModel *model, int nameRole)
	: QObject(model),
	  _model(model),
	  _nameRole(nameRole)
{
	connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this,
		&ModelChecker::RowsAboutToBeInserted);
	connect(model, &QAbstractItemModel::rowsInserted, this,
		&ModelChecker::RowsInserted);
	connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
		&ModelChecker::RowsAboutToBeRemoved);
	connect(model, &QAbstractItemModel::rowsRemoved, this,
		&ModelChecker::RowsRemoved);
	connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this,
		&ModelChecker::RowsAboutToBeMoved);
	connect(model, &QAbstractItemModel::rowsMoved, this,
		&ModelChecker::RowsMoved);
	connect(model, &QAbstractItemModel::modelReset, this,
		[this]() { CheckStructure(); });
	connect(model, &QAbstractItemModel::layoutChanged, this,
		[this]() { CheckStructure(); });
	CheckStructure();
}

void ModelChecker::CheckStructure()
{
	CheckStructure(QModelIndex(), 0);
}

void ModelChecker::CheckStructure(const QModelIndex &parent, int depth)
{
	if (depth > 100) {
		Error("model structure too deep - cycle?");
		return;
	}

	const int count = _model->rowCount(parent);
	for (int row = 0; row < count; row++) {
		const auto index = _model->index(row, 0, parent);
		if (!index.isValid()) {
			Error(QString("invalid index at row %1 below \"%2\"")
				      .arg(row)
				      .arg(Name(parent))
				      .toStdString());
			continue;
		}
		if (_model->parent(index) != parent) {
			Error(QString("parent of \"%1\" does not match \"%2\"")
				      .arg(Name(index))
				      .arg(Name(parent))
				      .toStdString());
		}
		if (Name(index).isEmpty()) {
			Error(QString("no name at row %1 below \"%2\"")
				      .arg(row)
				      .arg(Name(parent))
				      .toStdString());
		}
		CheckStructure(index, depth + 1);
	}
}

QString ModelChecker::Name(const QModelIndex &index) const
{
	if (!index.isValid()) {
		return "<root>";
	}
	return _model->data(index, _nameRole).toString();
}

void ModelChecker::Error(const std::string &message)
{
	_errors.emplace_back(message);
}

void ModelChecker::RowsAboutToBeInserted(const QModelIndex &parent, int, int)
{
	_pending = {};
	_pending.parent = parent;
	_pending.oldCount = _model->rowCount(parent);
}

void ModelChecker::RowsInserted(const QModelIndex &parent, int first, int last)
{
	const int expected = _pending.oldCount + last - first + 1;
	const int actual = _model->rowCount(parent);
	if (actual != expected) {
		Error(QString("insert below \"%1\": expected %2 rows, got %3")
			      .arg(Name(parent))
			      .arg(expected)
			      .arg(actual)
			      .toStdString());
	}
	CheckStructure();
}

void ModelChecker::RowsAboutToBeRemoved(const QModelIndex &parent, int first,
					int last)
{
	_pending = {};
	_pending.parent = parent;
	_pending.oldCount = _model->rowCount(parent);
	if (last >= _pending.oldCount || first < 0) {
		Error(QString("remove below \"%1\": rows %2-%3 out of range %4")
			      .arg(Name(parent))
			      .arg(first)
			      .arg(last)
			      .arg(_pending.oldCount)
			      .toStdString());
	}
}

void ModelChecker::RowsRemoved(const QModelIndex &parent, int first, int last)
{
	const int expected = _pending.oldCount - (last - first + 1);
	const int actual = _model->rowCount(parent);
	if (actual != expected) {
		Error(QString("remove below \"%1\": expected %2 rows, got %3")
			      .arg(Name(parent))
			      .arg(expected)
			      .arg(actual)
			      .toStdString());
	}
	CheckStructure();
}

void ModelChecker::RowsAboutToBeMoved(const QModelIndex &sourceParent,
				      int sourceStart, int sourceEnd,
				      const QModelIndex &destinationParent, int)
{
	_pending = {};
	_pending.parent = sourceParent;
	_pending.oldCount = _model->rowCount(sourceParent);
	_pending.destinationParent = destinationParent;
	_pending.oldDestinationCount = _model->rowCount(destinationParent);
	for (int row = sourceStart; row <= sourceEnd; row++) {
		_pending.names << Name(_model->index(row, 0, sourceParent));
	}
}

void ModelChecker::RowsMoved(const QModelIndex &sourceParent, int sourceStart,
			     int sourceEnd,
			     const QModelIndex &destinationParent,
			     int destinationRow)
{
	const int count = sourceEnd - sourceStart + 1;
	const bool sameParent = sourceParent == destinationParent;
	int newStart = destinationRow;
	if (sameParent && destinationRow > sourceEnd) {
		newStart -= count;
	}

	QStringList names;
	for (int row = newStart; row < newStart + count; row++) {
		names << Name(_model->index(row, 0, destinationParent));
	}
	if (names != _pending.names) {
		Error(QString("move to \"%1\" row %2: expected [%3], got [%4]")
			      .arg(Name(destinationParent))
			      .arg(destinationRow)
			      .arg(_pending.names.join(", "))
			      .arg(names.join(", "))
			      .toStdString());
	}

	if (!sameParent) {
		const int sourceCount = _model->rowCount(sourceParent);
		const int destinationCount =
			_model->rowCount(destinationParent);
		if (sourceCount != _pending.oldCount - count ||
		    destinationCount != _pending.oldDestinationCount + count) {
			Error(QString("move from \"%1\" to \"%2\": row counts do not match")
				      .arg(Name(sourceParent))
				      .arg(Name(destinationParent))
				      .toStdString());
		}
	} else if (_model->rowCount(sourceParent) != _pending.oldCount) {
		Error(QString("move within \"%1\": row count changed")
			      .arg(Name(sourceParent))
			      .toStdString());
	}
	CheckStructure();
}

} // namespace advss::test
