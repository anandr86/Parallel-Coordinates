
#include "ParallelCoordinates.h"
#include "QParallelCoordsData.h"


QParallelCoordsData::QParallelCoordsData(QObject *parent, const int axisCnt_) 
: QObject(parent), axis_cnt(-1), bulkUpdate(false)
{
	setAxisCount(axisCnt_);
}

int QParallelCoordsData::axis_count() const
{
	return axis_cnt;
}

void QParallelCoordsData::setAxisCount(int cnt)
{
	if(axis_cnt == -1) {
		axis_cnt = cnt;
		for(int i=0; i<axis_cnt; i++)
			axisData.push_back(qMakePair(QString(), qMakePair(std::numeric_limits<qreal>::max(),std::numeric_limits<qreal>::min())));
	}
}

void QParallelCoordsData::setRange(int axis_idx, QPair<qreal, qreal> range)
{
	axisData[axis_idx].second = range;
}

void QParallelCoordsData::addPoint(QVector<qreal> point)
{
	if(point.count() != axis_cnt)
		return;

	for(int i=0; i<point.count(); i++) {
		axisData[i].second.first = axisData[i].second.first > point[i] ? point[i] : axisData[i].second.first;
		axisData[i].second.second = axisData[i].second.second < point[i] ? point[i] : axisData[i].second.second;
	}

	data.append(point);
	if(!bulkUpdate) emit dataChanged(true);
}

void QParallelCoordsData::addPoints(QList<QVector<qreal>> pts)
{
	bulkUpdate = true;
	foreach(QVector<qreal> pt, pts) {
		addPoint(pt);
	}
	bulkUpdate = false;
	emit dataChanged(true);
}

QVector<qreal> const& QParallelCoordsData::operator[](int idx) const
{
	return data[idx];
}

int QParallelCoordsData::length() const
{
	return data.length();
}

QPair<qreal, qreal> QParallelCoordsData::getRange(int axis) const
{
	return axisData[axis].second;
}

void QParallelCoordsData::setAxisName(int idx, QString name)
{
	axisData[idx].first = name;
}

QString QParallelCoordsData::getAxisName(int idx)
{
	return axisData[idx].first;
}