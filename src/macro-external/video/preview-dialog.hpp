#pragma once
#include "paramerter-wrappers.hpp"

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QThread>
#include <QMouseEvent>
#include <QRubberBand>
#include <QPoint>
#include <mutex>

enum class PreviewType {
	SHOW_MATCH,
	SELECT_AREA,
};

class PreviewImage : public QObject {
	Q_OBJECT

public slots:
	void CreateImage(const VideoInput &, PreviewType,
			 const PatternMatchParameters &,
			 const PatternImageData &, ObjDetectParameters,
			 OCRParameters, const AreaParamters &, VideoCondition);
signals:
	void ImageReady(const QPixmap &);
	void StatusUpdate(const QString &);

private:
	void MarkMatch(QImage &screenshot, const PatternMatchParameters &,
		       const PatternImageData &, ObjDetectParameters &,
		       const OCRParameters &, VideoCondition);
};

class PreviewDialog : public QDialog {
	Q_OBJECT

public:
	PreviewDialog(QWidget *parent);
	virtual ~PreviewDialog();
	void ShowMatch();
	void SelectArea();
	void Stop();
	void closeEvent(QCloseEvent *event) override;

public slots:
	void PatternMatchParamtersChanged(const PatternMatchParameters &);
	void ObjDetectParamtersChanged(const ObjDetectParameters &);
	void OCRParamtersChanged(const OCRParameters &);
	void VideoSelectionChanged(const VideoInput &);
	void AreaParamtersChanged(const AreaParamters &);
	void ConditionChanged(int cond);
private slots:
	void UpdateImage(const QPixmap &);
	void UpdateStatus(const QString &);
signals:
	void SelectionAreaChanged(QRect area);
	void NeedImage(const VideoInput &, PreviewType,
		       const PatternMatchParameters &, const PatternImageData &,
		       ObjDetectParameters, OCRParameters,
		       const AreaParamters &, VideoCondition);

private:
	void Start();
	void DrawFrame();

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	VideoInput _video;
	PatternMatchParameters _patternMatchParams;
	PatternImageData _patternImageData;
	ObjDetectParameters _objDetectParams;
	OCRParameters _ocrParams;
	AreaParamters _areaParams;

	VideoCondition _condition = VideoCondition::PATTERN;
	QScrollArea *_scrollArea;
	QLabel *_statusLabel;
	QLabel *_imageLabel;

	QPoint _origin;
	QRubberBand *_rubberBand = nullptr;
	std::atomic_bool _selectingArea = {false};

	PreviewType _type = PreviewType::SHOW_MATCH;

	std::mutex _mtx;
	QThread _thread;
};
