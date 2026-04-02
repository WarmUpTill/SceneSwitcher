#pragma once
#include "source-interaction-step.hpp"
#include "source-interaction-step-list.hpp"
#include "source-preview-widget.hpp"

#include <chrono>
#include <vector>

#include <QDialog>
#include <QPushButton>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace advss {

class SourceInteractionRecorder : public QDialog {
	Q_OBJECT
public:
	SourceInteractionRecorder(QWidget *parent, obs_weak_source_t *source);
	~SourceInteractionRecorder();

	const std::vector<SourceInteractionStep> &GetSteps() const
	{
		return _steps;
	}

signals:
	void StepsRecorded(const std::vector<SourceInteractionStep> &);

private slots:
	void StartStop();

private:
	bool eventFilter(QObject *obj, QEvent *event) override;
	bool HandleMouseClick(QMouseEvent *);
	bool HandleMouseMove(QMouseEvent *);
	bool HandleMouseWheel(QWheelEvent *);
	bool HandleKeyEvent(QKeyEvent *);
	void AppendStep(const SourceInteractionStep &);

	obs_weak_source_t *_source;
	SourcePreviewWidget *_preview;
	SourceInteractionStepList *_stepList;
	QPushButton *_startStopButton;

	bool _recording = false;
	std::vector<SourceInteractionStep> _steps;
	std::chrono::steady_clock::time_point _lastEventTime;
	bool _firstEvent = true;
};

} // namespace advss
