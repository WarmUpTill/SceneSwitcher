#pragma once
#include "paramerter-wrappers.hpp"

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>

#include <QMouseEvent>
#include <QRubberBand>
#include <QPoint>

#include <thread>
#include <mutex>
#include <condition_variable>

class PreviewDialog : public QDialog {
	Q_OBJECT

public:
	PreviewDialog(QWidget *parent, int delay);
	virtual ~PreviewDialog();
	void ShowMatch();
	void SelectArea();
	void Stop();
	void closeEvent(QCloseEvent *event) override;

public slots:
	void PatternMatchParamtersChanged(const PatternMatchParameters &);
	void ObjDetectParamtersChanged(const ObjDetectParamerts &);
	void VideoSelectionChanged(const VideoInput &);
	void AreaParamtersChanged(const AreaParamters &);
	void ConditionChanged(int cond);
private slots:
	void ResizeImageLabel();
signals:
	void SelectionAreaChanged(QRect area);

private:
	void Start();
	void CheckForMatchLoop();
	void MarkMatch(QImage &screenshot);
	void DrawFrame();

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	VideoInput _video;
	PatternMatchParameters _patternMatchParams;
	PatternImageData _patternImageData;
	ObjDetectParamerts _objDetectParams;
	AreaParamters _areaParams;

	VideoCondition _condition = VideoCondition::PATTERN;
	QScrollArea *_scrollArea;
	QLabel *_statusLabel;
	QLabel *_imageLabel;
	QTimer _timer;

	QPoint _origin;
	QRubberBand *_rubberBand = nullptr;
	std::atomic_bool _selectingArea = {false};

	std::mutex _mtx;
	std::condition_variable _cv;
	std::thread _thread;
	std::atomic_bool _stop = {true};

	enum class Type {
		SHOW_MATCH,
		SELECT_AREA,
	};
	Type _type = Type::SHOW_MATCH;
	int _delay = 300;
};
