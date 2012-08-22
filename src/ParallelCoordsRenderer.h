#ifndef __PARALLELCOORDS_RENDERER_H__
#define __PARALLELCOORDS_RENDERER_H__

#include "ParallelCoordinates.h"
#include "ParallelCoordsViewPrivate.h"

class ParallelCoordsRenderer_worker : public QObject {
	Q_OBJECT

public:
	ParallelCoordsRenderer_worker();

public slots:
	void renderImage(
		QVector<QPolygonF> *polyLines, 
		QVector<pointProcessData_> *ppd, 
		QRectF visible_rect, 
		QSizeF viewportSize);

signals:
	void drawRequest(QImage *img);

private:
	int threadingThreshold;	// No of line segments above which threading starts
	int axisAnchorMargin;
	int axisWidth;
};

class ParallelCoordsRenderer : public QThread {
	Q_OBJECT
public:
	ParallelCoordsRenderer(QObject *parent);

protected:
	void run();

private:
	ParallelCoordsRenderer_worker *worker;
};

#endif