#include "source-interaction-recorder.hpp"
#include "obs-module-helper.hpp"

#include <obs.hpp>
#include <obs-interaction.h>

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

namespace advss {

SourceInteractionRecorder::SourceInteractionRecorder(QWidget *parent,
						     obs_weak_source_t *source)
	: QDialog(parent),
	  _source(source),
	  _preview(new SourcePreviewWidget(this, source)),
	  _stepList(new SourceInteractionStepList(this)),
	  _startStopButton(new QPushButton(
		  obs_module_text(
			  "AdvSceneSwitcher.action.sourceInteraction.record.start"),
		  this))
{
	setWindowTitle(obs_module_text(
		"AdvSceneSwitcher.action.sourceInteraction.record.title"));

	_preview->setMouseTracking(true);
	_preview->setFocusPolicy(Qt::StrongFocus);
	_preview->installEventFilter(this);

	_stepList->HideControls();
	_stepList->SetMinListHeight(50);

	auto buttonRow = new QHBoxLayout;
	buttonRow->addWidget(_startStopButton);
	buttonRow->addStretch();

	auto acceptButton = new QPushButton(
		obs_module_text(
			"AdvSceneSwitcher.action.sourceInteraction.record.accept"),
		this);
	buttonRow->addWidget(acceptButton);

	auto layout = new QVBoxLayout(this);
	layout->addWidget(_preview, 1);
	layout->addWidget(_stepList);
	layout->addLayout(buttonRow);
	setLayout(layout);

	connect(_startStopButton, &QPushButton::clicked, this,
		&SourceInteractionRecorder::StartStop);
	connect(acceptButton, &QPushButton::clicked, this, [this]() {
		if (_recording) {
			StartStop();
		}
		emit StepsRecorded(_steps);
		accept();
	});
}

SourceInteractionRecorder::~SourceInteractionRecorder() {}

void SourceInteractionRecorder::StartStop()
{
	_recording = !_recording;
	if (_recording) {
		_steps.clear();
		_stepList->Clear();
		_firstEvent = true;
		_startStopButton->setText(obs_module_text(
			"AdvSceneSwitcher.action.sourceInteraction.record.stop"));
	} else {
		_startStopButton->setText(obs_module_text(
			"AdvSceneSwitcher.action.sourceInteraction.record.start"));
	}
}

void SourceInteractionRecorder::AppendStep(const SourceInteractionStep &step)
{
	if (!_firstEvent) {
		auto now = std::chrono::steady_clock::now();
		int ms = static_cast<int>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				now - _lastEventTime)
				.count());
		if (ms > 10) {
			SourceInteractionStep wait;
			wait.type = SourceInteractionStep::Type::WAIT;
			wait.waitMs.SetValue(ms);
			_steps.push_back(wait);
			_stepList->Insert(
				QString::fromStdString(wait.ToString()));
		}
	}
	_firstEvent = false;
	_lastEventTime = std::chrono::steady_clock::now();

	_steps.push_back(step);
	_stepList->Insert(QString::fromStdString(step.ToString()));
	_stepList->ScrollToBottom();
}

bool SourceInteractionRecorder::eventFilter(QObject *obj, QEvent *event)
{
	if (obj != _preview || !_recording) {
		return QDialog::eventFilter(obj, event);
	}

	switch (event->type()) {
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
		return HandleMouseClick(static_cast<QMouseEvent *>(event));
	case QEvent::MouseMove:
		return HandleMouseMove(static_cast<QMouseEvent *>(event));
	case QEvent::Wheel:
		return HandleMouseWheel(static_cast<QWheelEvent *>(event));
	case QEvent::KeyPress:
	case QEvent::KeyRelease:
		return HandleKeyEvent(static_cast<QKeyEvent *>(event));
	case QEvent::FocusIn:
	case QEvent::FocusOut: {
		OBSSourceAutoRelease source =
			obs_weak_source_get_source(_source);
		if (source) {
			obs_source_send_focus(source,
					      event->type() == QEvent::FocusIn);
		}
		return true;
	}
	default:
		break;
	}

	return QDialog::eventFilter(obj, event);
}

bool SourceInteractionRecorder::HandleMouseClick(QMouseEvent *event)
{
	SourceInteractionStep step;
	step.type = SourceInteractionStep::Type::MOUSE_CLICK;

	int srcX = 0, srcY = 0;
	QPoint pos = event->pos();
	_preview->GetSourceRelativeXY(pos.x(), pos.y(), srcX, srcY);
	step.x.SetValue(srcX);
	step.y.SetValue(srcY);

	step.mouseUp = (event->type() == QEvent::MouseButtonRelease);
	step.clickCount.SetValue(
		(event->type() == QEvent::MouseButtonDblClick) ? 2 : 1);

	switch (event->button()) {
	case Qt::LeftButton:
		step.button = MOUSE_LEFT;
		break;
	case Qt::MiddleButton:
		step.button = MOUSE_MIDDLE;
		break;
	case Qt::RightButton:
		step.button = MOUSE_RIGHT;
		break;
	default:
		return false;
	}

	AppendStep(step);

	OBSSourceAutoRelease source = obs_weak_source_get_source(_source);
	if (source) {
		PerformInteractionStep(source, step);
	}
	return true;
}

bool SourceInteractionRecorder::HandleMouseMove(QMouseEvent *event)
{
	SourceInteractionStep step;
	step.type = SourceInteractionStep::Type::MOUSE_MOVE;
	int srcX = 0, srcY = 0;
	QPoint pos = event->pos();
	_preview->GetSourceRelativeXY(pos.x(), pos.y(), srcX, srcY);
	step.x.SetValue(srcX);
	step.y.SetValue(srcY);

	// Always forward moves to the source so hover/cursor effects work,
	// but only record them when a button is held (drag), to reduce noise.
	OBSSourceAutoRelease source = obs_weak_source_get_source(_source);
	if (source) {
		PerformInteractionStep(source, step);
	}

	if (event->buttons() != Qt::NoButton) {
		AppendStep(step);
	}
	return true;
}

bool SourceInteractionRecorder::HandleMouseWheel(QWheelEvent *event)
{
	SourceInteractionStep step;
	step.type = SourceInteractionStep::Type::MOUSE_WHEEL;
	int srcX = 0, srcY = 0;
	const QPointF pos = event->position();
	_preview->GetSourceRelativeXY((int)pos.x(), (int)pos.y(), srcX, srcY);
	step.x.SetValue(srcX);
	step.y.SetValue(srcY);
	const QPoint angle = event->angleDelta();
	step.wheelDeltaX.SetValue(angle.x());
	step.wheelDeltaY.SetValue(angle.y());
	AppendStep(step);

	OBSSourceAutoRelease source = obs_weak_source_get_source(_source);
	if (source) {
		PerformInteractionStep(source, step);
	}
	return true;
}

bool SourceInteractionRecorder::HandleKeyEvent(QKeyEvent *event)
{
	SourceInteractionStep step;
	step.type = SourceInteractionStep::Type::KEY_PRESS;
	step.keyUp = (event->type() == QEvent::KeyRelease);
	step.nativeVkey.SetValue(static_cast<int>(event->nativeVirtualKey()));
	step.text = event->text().toStdString();
	AppendStep(step);

	OBSSourceAutoRelease source = obs_weak_source_get_source(_source);
	if (source) {
		PerformInteractionStep(source, step);
	}
	return true;
}

} // namespace advss
