
#ifndef __PARALLELCOORDSRENDERMANAGER_H__
#define __PARALLELCOORDSRENDERMANAGER_H__

#include "ParallelCoordinates.h"
#include "QParallelCoordsData.h"
#include "ParallelCoordsViewPrivate.h"

class ParallelCoordsRenderManager : public QObject
{
	Q_OBJECT
public:
	ParallelCoordsRenderManager(QSize canvasSize,
								QPair<qreal, qreal> scaleFactors,
								QSize viewportSize,
								QList<axis_view_data> const *axis_data,
								QParallelCoordsData const* data);

public slots:
	void getTile(QRect rect);
	void viewportSizeChange(QSize viewportSize);
	void scaleFactorsChange(QPair<qreal, qreal> scaleFactors);
	void canvasSizeChange(QSize canvasSize);
	void axisDataChange();

signals:
	void tileGenerated(QRect r, QImage *img);

private:
	QSize canvasSize;
	QPair<qreal, qreal> scaleFactors;
	QSize viewportSize;
	QHash<QRect,QImage*> imgCache;
	QParallelCoordsData const *data;
	int threadingThreshold;
	int axisPenWidth;

	void filterData(
		QRectF visible_rect,
		QVector<renderData> **ppd_ptr,
		QVector<QPolygonF> **polyLineSet_ptr);
	QImage* renderImage(
		QVector<QPolygonF> *polyLineSet, 
		QVector<renderData> *ppd, 
		QRectF visible_rect, QSizeF parentViewportSize);
	QList<axis_view_data> const *axis_data;

	void flushCache();

};

#endif