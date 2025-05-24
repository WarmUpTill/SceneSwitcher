#pragma once
#include "parameter-wrappers.hpp"

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QThread>
#include <QMouseEvent>
#include <QRubberBand>
#include <QPoint>
#include <mutex>

namespace advss {

enum class PreviewType {
	SHOW_MATCH,
	SELECT_AREA,
};

class PreviewImage : public QObject {
	Q_OBJECT

public:
	PreviewImage(std::mutex &);

public slots:
	void CreateImage(const VideoInput &, PreviewType,
			 const PatternMatchParameters &,
			 const PatternImageData &,
			 std::shared_ptr<ObjDetectParameters>,
			 std::shared_ptr<OCRParameters>, const AreaParameters &,
			 VideoCondition);
signals:
	void ImageReady(const QPixmap &);
	void ValueUpdate(double);
	void StatusUpdate(const QString &);

private:
	void MarkMatch(QImage &screenshot, const PatternMatchParameters &,
		       const PatternImageData &,
		       std::shared_ptr<ObjDetectParameters>,
		       std::shared_ptr<OCRParameters>, VideoCondition);
	void MarkPatternMatch(QImage &, const PatternMatchParameters &,
			      const PatternImageData &);
	void MarkObjectMatch(QImage &,
			     const std::shared_ptr<ObjDetectParameters> &);
	void MarkOCRMatch(QImage &, const std::shared_ptr<OCRParameters> &);

	std::mutex &_mtx;
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
	void PatternMatchParametersChanged(const PatternMatchParameters &);
	void ObjDetectParametersChanged(const ObjDetectParameters &);
	void OCRParametersChanged(const OCRParameters &);
	void VideoSelectionChanged(const VideoInput &);
	void AreaParametersChanged(const AreaParameters &);
	void ConditionChanged(int cond);
private slots:
	void UpdateImage(const QPixmap &);
	void UpdateValue(double);
	void UpdateStatus(const QString &);
signals:
	void SelectionAreaChanged(QRect area);
	void NeedImage(const VideoInput &, PreviewType,
		       const PatternMatchParameters &, const PatternImageData &,
		       std::shared_ptr<ObjDetectParameters>,
		       std::shared_ptr<OCRParameters>, const AreaParameters &,
		       VideoCondition);

private:
	void Start();
	void DrawFrame();

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	VideoInput _video;
	PatternMatchParameters _patternMatchParams;
	PatternImageData _patternImageData;
	std::shared_ptr<ObjDetectParameters> _objDetectParams;
	std::shared_ptr<OCRParameters> _ocrParams;
	AreaParameters _areaParams;

	VideoCondition _condition = VideoCondition::PATTERN;
	QScrollArea *_scrollArea;
	QLabel *_valueLabel;
	QLabel *_statusLabel;
	QLabel *_imageLabel;

	QPoint _origin;
	QRubberBand *_rubberBand = nullptr;
	std::atomic_bool _selectingArea = {false};

	PreviewType _type = PreviewType::SHOW_MATCH;

	std::mutex _mtx;
	QThread _thread;
};

} // namespace advss
