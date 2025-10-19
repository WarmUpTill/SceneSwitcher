
#pragma once
#include "export-symbol-helper.hpp"

#include <QObject>
#include <QString>

namespace advss {

class ADVSS_EXPORT MacroSignalManager : public QObject {
	Q_OBJECT
public:
	static MacroSignalManager *Instance();

signals:
	void Rename(const QString &, const QString &);
	void Add(const QString &);
	void Remove(const QString &);
	void SegmentOrderChanged();
	void HighlightChanged(bool value);

private:
	MacroSignalManager(QObject *parent = nullptr);
};

} // namespace advss
