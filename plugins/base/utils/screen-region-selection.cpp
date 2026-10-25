#include "screen-region-selection.hpp"
#include "obs-module-helper.hpp"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QWindow>

namespace advss {

ScreenRegionSelection::ScreenRegionSelection(QWidget *parent) : QObject(parent)
{
	QWidget *activeOverlay = nullptr;
	const auto cursor = QCursor::pos();
	for (auto screen : QGuiApplication::screens()) {
		auto overlay = std::make_unique<QWidget>(
			parent->window(), Qt::Tool | Qt::FramelessWindowHint |
						  Qt::WindowStaysOnTopHint);
		// Keep native overlay windows separate from the macro's widgets.
		overlay->setAttribute(Qt::WA_DontCreateNativeAncestors);
		overlay->setAttribute(Qt::WA_NoMousePropagation);
		overlay->setAttribute(Qt::WA_TranslucentBackground);
		overlay->setCursor(Qt::CrossCursor);
		overlay->setFocusPolicy(Qt::StrongFocus);
		overlay->setMouseTracking(true);
		overlay->installEventFilter(this);
		// Each monitor needs its own window for mixed display scaling.
		overlay->winId();
		overlay->windowHandle()->setScreen(screen);
		overlay->setGeometry(screen->geometry());
		overlay->show();
		if (screen->geometry().contains(cursor)) {
			activeOverlay = overlay.get();
		}
		_overlays.emplace_back(std::move(overlay));
	}
	if (activeOverlay) {
		activeOverlay->raise();
		activeOverlay->activateWindow();
		activeOverlay->setFocus();
	}
	connect(qGuiApp, &QGuiApplication::screenRemoved, this,
		[this]() { Finish(false); });
	connect(qGuiApp, &QGuiApplication::screenAdded, this,
		[this]() { Finish(false); });
	connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
		[this](Qt::ApplicationState state) {
			if (state != Qt::ApplicationActive) {
				Finish(false);
			}
		});
}

ScreenRegionSelection::~ScreenRegionSelection()
{
	for (auto &overlay : _overlays) {
		overlay->removeEventFilter(this);
	}
}

bool ScreenRegionSelection::eventFilter(QObject *watched, QEvent *event)
{
	if (_finished) {
		return false;
	}
	auto overlay = static_cast<QWidget *>(watched);
	switch (event->type()) {
	case QEvent::Paint: {
		QPainter painter(overlay);
		painter.fillRect(overlay->rect(), QColor(0, 0, 0, 70));
		if (_drawing) {
			const QRect region(
				overlay->mapFromGlobal(_selection.topLeft()),
				overlay->mapFromGlobal(
					_selection.bottomRight()));
			painter.fillRect(region, QColor(60, 150, 255, 60));
			painter.setPen(QPen(Qt::white, 2));
			painter.drawRect(region.adjusted(0, 0, -1, -1));
		}
		painter.setPen(Qt::white);
		painter.drawText(
			overlay->rect().adjusted(24, 24, -24, -24),
			Qt::AlignTop | Qt::AlignHCenter | Qt::TextWordWrap,
			obs_module_text(
				"AdvSceneSwitcher.condition.cursor.drawFrame.prompt"));
		return true;
	}
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonDblClick: {
		event->accept();
		auto mouse = static_cast<QMouseEvent *>(event);
		if (mouse->button() == Qt::RightButton) {
			Finish(false);
		} else if (mouse->button() == Qt::LeftButton) {
			_start = QCursor::pos();
			_drawing = true;
			UpdateSelection();
		}
		return true;
	}
	case QEvent::MouseMove:
		event->accept();
		if (_drawing) {
			UpdateSelection();
		}
		return true;
	case QEvent::MouseButtonRelease:
		event->accept();
		if (_drawing && static_cast<QMouseEvent *>(event)->button() ==
					Qt::LeftButton) {
			UpdateSelection();
			if (_selection.width() > 1 && _selection.height() > 1) {
				Finish(true);
			} else {
				_drawing = false;
				UpdateSelection();
			}
		}
		return true;
	case QEvent::KeyPress:
		event->accept();
		if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
			Finish(false);
		}
		return true;
	case QEvent::ShortcutOverride:
		event->accept();
		return true;
	case QEvent::Close:
		Finish(false);
		return true;
	default:
		return QObject::eventFilter(watched, event);
	}
}

void ScreenRegionSelection::UpdateSelection()
{
	// Use the same global coordinate system as the Cursor condition.
	_selection = QRect(_start, QCursor::pos()).normalized();
	for (auto &overlay : _overlays) {
		overlay->update();
	}
}

void ScreenRegionSelection::Finish(bool accept)
{
	if (_finished) {
		return;
	}
	_finished = true;
	for (auto &overlay : _overlays) {
		overlay->hide();
	}
	if (accept) {
		emit RegionSelected(_selection);
	}
	emit Finished();
	deleteLater();
}

} // namespace advss
