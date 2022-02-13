#pragma once

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <thread>

class MacroConditionVideo;

class ShowMatchDialog : public QDialog {
	Q_OBJECT

public:
	ShowMatchDialog(QWidget *parent, MacroConditionVideo *_conditionData);
	virtual ~ShowMatchDialog();
	void ShowMatch();

private slots:
	void RedrawImage(QImage img);

private:
	void CheckForMatchLoop();
	QImage MarkMatch(QImage &screenshot);

	MacroConditionVideo *_conditionData;
	QScrollArea *_scrollArea;
	QLabel *_statusLabel;
	QLabel *_imageLabel;
	std::thread _thread;
	std::atomic_bool _stop = {false};
};
