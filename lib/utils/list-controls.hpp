#pragma once
#include "export-symbol-helper.hpp"

#include <QToolBar>

namespace advss {

class ADVSS_EXPORT ListControls final : public QToolBar {
	Q_OBJECT

public:
	ListControls(QWidget *parent = nullptr, bool reorder = true);

signals:
	void Add();
	void Remove();
	void Up();
	void Down();

private:
	void AddActionHelper(const char *theme, const char *tooltip,
			     const std::function<void()> &signal);
};

} // namespace advss
