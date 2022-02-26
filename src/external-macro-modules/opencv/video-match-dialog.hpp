#pragma once

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <thread>
#include <mutex>

class MacroConditionVideo;

class ShowMatchDialog : public QDialog {
	Q_OBJECT

public:
	ShowMatchDialog(QWidget *parent, MacroConditionVideo *_conditionData,
			std::mutex *mutex);
	virtual ~ShowMatchDialog();
	void ShowMatch();

private slots:
	void Resize();

private:
	void CheckForMatchLoop();
	QImage MarkMatch(QImage &screenshot);

	MacroConditionVideo *_conditionData;
	QScrollArea *_scrollArea;
	QLabel *_statusLabel;
	QLabel *_imageLabel;
	QTimer _timer;
	std::mutex *_mtx;
	std::thread _thread;
	std::atomic_bool _stop = {false};
};
