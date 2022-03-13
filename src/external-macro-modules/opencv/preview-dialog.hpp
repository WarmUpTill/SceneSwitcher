#pragma once

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>

#include <QMouseEvent>
#include <QRubberBand>
#include <QPoint>

#include <thread>
#include <mutex>

class MacroConditionVideo;

class PreviewDialog : public QDialog {
	Q_OBJECT

public:
	PreviewDialog(QWidget *parent, MacroConditionVideo *_conditionData,
		      std::mutex *mutex);
	virtual ~PreviewDialog();
	void ShowMatch();
	void SelectArea();

private slots:
	void Resize();
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

	MacroConditionVideo *_conditionData;
	QScrollArea *_scrollArea;
	QLabel *_statusLabel;
	QLabel *_imageLabel;
	QTimer _timer;

	QPoint _origin;
	QRubberBand *_rubberBand = nullptr;
	std::atomic_bool _selectingArea = {false};

	std::mutex *_mtx;
	std::thread _thread;
	std::atomic_bool _stop = {false};

	enum class Type {
		SHOW_MATCH,
		SELECT_AREA,
	};
	Type _type = Type::SHOW_MATCH;
};
