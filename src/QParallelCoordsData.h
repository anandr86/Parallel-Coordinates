
#ifndef __QPARALLELCOORDSDATA_H__
#define __QPARALLELCOORDSDATA_H__

#include "ParallelCoordinates.h"

class QParallelCoordsData : public QObject {

	Q_OBJECT

public:
	QParallelCoordsData(QObject *parent, int axisCnt_=-1);
	void addPoint(QVector<qreal> point);
	void addPoints(QList<QVector<qreal>> pts);
	QVector<qreal> const& operator[](int idx) const;
	int length() const;
	QPair<qreal, qreal> getRange(int axis) const;
	void setRange(int axis_idx, QPair<qreal, qreal> range);
	void setRange(int start_idx, QList<QPair<qreal, qreal>> const& ranges);
	int axis_count() const;
	void setAxisCount(int cnt);
	QString getAxisName(int idx);
	void setAxisName(int idx, QString name);

private:
	int axis_cnt;
	QList<QVector<qreal>> data;
	QList<QPair<QString,QPair<qreal, qreal>>> axisData;
	bool bulkUpdate;

signals:
	void dataChanged(bool);
};

#endif