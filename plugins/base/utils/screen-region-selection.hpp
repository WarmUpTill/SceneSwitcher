#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <memory>
#include <vector>

namespace advss {

class ScreenRegionSelection : public QObject {
	Q_OBJECT

public:
	explicit ScreenRegionSelection(QWidget *parent);
	~ScreenRegionSelection() override;

signals:
	void RegionSelected(const QRect &region);
	void Finished();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void UpdateSelection();
	void Finish(bool accept);

	std::vector<std::unique_ptr<QWidget>> _overlays;
	QPoint _start;
	QRect _selection;
	bool _drawing = false;
	bool _finished = false;
};

} // namespace advss
