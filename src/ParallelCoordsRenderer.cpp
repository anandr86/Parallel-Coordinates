
#include "ParallelCoordinates.h"
#include "ParallelCoordsRenderer.h"
#include <functional>
#include <algorithm>

static void renderPolylines(QImage *img, 
	QRectF visible_rect, 
	QVector<QPolygonF> const *polyLineSet);

static QVector<QLineF> 
selectVisibleSegments(QPolygonF polyLine, QRectF visible_rect);

QVector<QLineF> selectVisibleSegments(QPolygonF polyLine, QRectF visible_rect)
{
	QVector<QLineF> segments;

	// Build individual segments
	// Filter the segments
	// Return lines	
	for(auto it=polyLine.begin()+1; it != polyLine.end(); it++) {
		QPointF pt1(*(it-1));
		QPointF pt2(*it);
		QLineF l(*(it-1), *it);
		QRectF r(qMin(pt1.x(), pt2.x()),
			     qMin(pt1.y(), pt2.y()),
			     qMax(pt1.x(), pt2.x()),
			     qMax(pt1.y(), pt2.y()));
		if(visible_rect.intersects(r))
			segments.push_back(l);
	}

	return segments;
}

// Render on to img, the specified region on the canvas described
void renderPolylines(QImage *img, QRectF visible_rect, 
	QVector<QPolygonF> const *polyLineSet)
{
	// Filter lines that have both ends out of view
	QVector<QLineF> segments;

	using namespace std::placeholders;
	auto result = QtConcurrent::mapped(*polyLineSet, 
		std::bind(selectVisibleSegments, _1, visible_rect));
	result.waitForFinished();

	for(auto it=result.constBegin(); it != result.constEnd(); it++) {
		for(auto i=(*it).begin(); i != (*it).end(); i++)
			segments.push_back(*i);
	}

	// setup image, painter, transforms and clipping region
	// draw to bitmap
	/*
	* ====================   Important note    =====================
	* Don't change order of scale, translate and setClipRect
	* painter applies the latest function called first
	* So we are clipping first
	* then we are translating 
	* and finally scaling
	* Try working out a few numeirc examples in minds before attempting 
	* to change the order of the numbers
	*/

	QSizeF viewportSize = img->size();
	img->fill(QColor(255,255,255));
	QPainter painter;
	painter.begin(img);
	painter.scale(viewportSize.width()/visible_rect.width(), 
		viewportSize.height()/visible_rect.height());
	painter.translate(visible_rect.topLeft() * -1);
	painter.setClipRect(visible_rect);

	//draw all the axis lines
	QPen linePen;
	linePen.setWidthF(0);
	painter.setPen(linePen);
	painter.drawLines(segments);
	painter.end();
}

ParallelCoordsRenderer::ParallelCoordsRenderer(QObject *parent)
: QThread(parent)
{}

void ParallelCoordsRenderer::run()
{
	worker = new ParallelCoordsRenderer_worker;
	connect(parent(), 
		SIGNAL(renderRequest(QVector<QPolygonF>*,
							 QVector<pointProcessData_>*,
							 QRectF,QSizeF)),
		worker,
		SLOT(renderImage(QVector<QPolygonF>*,
						 QVector<pointProcessData_>*,
						 QRectF,QSizeF)));
	connect(worker,
		SIGNAL(drawRequest(QImage*)),
		parent(),
		SLOT(drawImage(QImage*)));
	exec();
}

ParallelCoordsRenderer_worker::ParallelCoordsRenderer_worker()
{
	threadingThreshold = 15000;
	axisAnchorMargin = 20;
	axisWidth = 2;
}

void ParallelCoordsRenderer_worker::renderImage(
	QVector<QPolygonF> *polyLineSet, 
	QVector<pointProcessData_> *ppd, 
	QRectF visible_rect, QSizeF parentViewportSize)
{
	QSizeF viewportSize = parentViewportSize;
	viewportSize.setHeight(viewportSize.height() - 2.0 * axisAnchorMargin);

	QRectF zone1, zone2, zone3, zone4;
	zone1 = visible_rect;
	zone1.setBottomRight(visible_rect.bottomRight()/2.0);
	zone2 = zone3 = zone4 = zone1;
	zone2.translate(zone1.width(), 0.0);
	zone3.translate(0.0, zone1.height());
	zone4.translate(zone1.bottomRight());
	int zoneCnt[4] = {0};

	const int relevantAxisCnt = ppd->count();
	const int polyLineCnt = polyLineSet->count();

	for(int i=0; i<polyLineCnt; i++) {
		for(int j=0; j<relevantAxisCnt; j++) {
			if(zone1.contains((*polyLineSet)[i][j]))
				zoneCnt[0]++;
			else if(zone2.contains((*polyLineSet)[i][j]))
				zoneCnt[1]++;
			else if(zone3.contains((*polyLineSet)[i][j]))
				zoneCnt[2]++;
			else if(zone4.contains((*polyLineSet)[i][j]))
				zoneCnt[3]++;
		}
	}
	int horizontal_bias = 
	qAbs((zoneCnt[0] + zoneCnt[1]) - (zoneCnt[2] + zoneCnt[3]));
	int vertical_bias = 
	qAbs((zoneCnt[0] + zoneCnt[2]) - (zoneCnt[1] + zoneCnt[3]));

	QImage *img = new QImage(viewportSize.toSize(), QImage::Format_ARGB32_Premultiplied);

	if(polyLineSet->count()) {
		if((polyLineCnt * (relevantAxisCnt-1) < threadingThreshold)) {
			renderPolylines(img, visible_rect, polyLineSet);
		}
		else { // Now we are going to use threads and paint
			if(horizontal_bias < vertical_bias) { // split horizontally
				QImage *img1 = new QImage(viewportSize.width(), 
					viewportSize.height()/2.0, QImage::Format_ARGB32_Premultiplied); 
				QImage *img2 = new QImage(viewportSize.width(), 
					viewportSize.height()/2.0, QImage::Format_ARGB32_Premultiplied);
				QRectF rect1, rect2;
				rect1 = visible_rect;
				rect1.setHeight(visible_rect.height()/2.0);
				rect2 = rect1;
				rect2.translate(0.0, rect1.height());

				auto r = QtConcurrent::run(renderPolylines, 
										   img1, 
										   rect1, 
										   polyLineSet);
				renderPolylines(img2, rect2, polyLineSet);
				r.waitForFinished();

				QPainter painter(img);
				painter.drawImage(0, 0, *img1);
				painter.drawImage(0,viewportSize.height()/2.0, *img2);

				delete img1;
				delete img2;
			}
			else { // split vertically
				QImage *img1 = new QImage(viewportSize.width()/2.0, 
					viewportSize.height(), QImage::Format_ARGB32_Premultiplied);
				QImage *img2 = new QImage(viewportSize.width()/2.0, 
					viewportSize.height(), QImage::Format_ARGB32_Premultiplied);
				QRectF rect1, rect2;
				rect1 = visible_rect;
				rect1.setWidth(visible_rect.width()/2.0);
				rect2 = rect1;
				rect2.translate(rect1.width(), 0.0);

				auto r = QtConcurrent::run(renderPolylines, 
										   img1, 
										   rect1, 
										   polyLineSet);
				renderPolylines(img2, rect2, polyLineSet);
				r.waitForFinished();

				QPainter painter(img);
				painter.drawImage(0, 0, *img1);
				painter.drawImage(viewportSize.width()/2.0, 0, *img2);

				delete img1;
				delete img2;
			}
		}
	}

	// draw all axis
	// We want a constant width axis at all scale levels
	// so we do all the scaling and translation manually before
	// drawing. 
	QTransform t;
	t.scale(viewportSize.width()/visible_rect.width(),
			viewportSize.height()/visible_rect.height());
	t.translate(visible_rect.left() * -1, visible_rect.top() * -1);
	QVector<QLineF> axisLines;
	for(int i=0; i<relevantAxisCnt; i++) {
		QLineF axis(
			t.map(QPointF((*ppd)[i].axis_x, (*ppd)[i].axis_y)),
			t.map(QPointF((*ppd)[i].axis_x, 
						  (*ppd)[i].axis_y + (*ppd)[i].axis_height)));
		axisLines.push_back(axis);
	}

	QPainter painter(img);
	QPen axisPen;
	axisPen.setWidthF(axisWidth);
	axisPen.setColor(QColor("#FF0000"));
	painter.setPen(axisPen);
	painter.drawLines(axisLines);
	painter.end();

	//draw axis anchors above and below appropriately
	QVector<qreal> axisOffset;
	for(int i=0; i<relevantAxisCnt; i++) {
		QPointF pt = t.map(QPointF((*ppd)[i].axis_x, (*ppd)[i].axis_y));
		axisOffset << 
		(pt.x() * parentViewportSize.width() / viewportSize.width());
	}

	QImage *imgF = new QImage(parentViewportSize.toSize(), 
							  QImage::Format_ARGB32_Premultiplied);
	imgF->fill(QColor(255,255,255));
	painter.begin(imgF);
	axisPen.setWidthF(0);
	axisPen.setColor(QColor("#FF0000"));
	painter.setPen(axisPen);

	QBrush b(painter.brush());
	b.setColor(QColor("#FF0000"));
	b.setStyle(Qt::SolidPattern);
	painter.setBrush(b);

	painter.drawImage(0, axisAnchorMargin, *img);
	foreach(qreal x, axisOffset) {
		painter.drawRect(x-9, 
						 0, 
						 axisAnchorMargin, 
						 axisAnchorMargin);
		painter.drawRect(x-9, 
						 parentViewportSize.height()-axisAnchorMargin, 
						 axisAnchorMargin, 
						 axisAnchorMargin);
	}
	painter.end();

	delete polyLineSet;
	delete ppd;
	delete img;

	emit drawRequest(imgF);
}