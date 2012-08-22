
#include "ParallelCoordinates.h"
#include "ParallelCoordsRenderManager.h"
#include <functional>

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
	{
		bool stat = painter.begin(img);
		Q_ASSERT(stat);
	}
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

uint qHash(QRect rect)
{
	return qHash(QString("%1 %2").arg(rect.x())
								 .arg(rect.y()));
}

ParallelCoordsRenderManager::ParallelCoordsRenderManager(
	QSize canvasSize_,
	QPair<qreal, qreal> scaleFactors_,
	QSize viewportSize_,
	QList<axis_view_data> const *axis_data_,
	QParallelCoordsData const *data_)
: data(data_), axis_data(axis_data_)
{
	canvasSize = canvasSize_;
	scaleFactors = scaleFactors_;
	viewportSize = viewportSize_;
	threadingThreshold = 15000;
	axisPenWidth = 2;
}

void ParallelCoordsRenderManager::flushCache()
{
	foreach(QImage* i, imgCache.values()) {
		delete i;
	}
	imgCache.clear();
}

void ParallelCoordsRenderManager::viewportSizeChange(QSize viewportSize_)
{
	viewportSize = viewportSize_;
	imgCache.clear();
}

void ParallelCoordsRenderManager::canvasSizeChange(QSize canvasSize_)
{
	canvasSize = canvasSize_;
	imgCache.clear();
}

void ParallelCoordsRenderManager::scaleFactorsChange(
	QPair<qreal, qreal> scaleFactors_)
{
	scaleFactors = scaleFactors_;
	imgCache.clear();
}

void ParallelCoordsRenderManager::axisDataChange()
{
	imgCache.clear();
}

void ParallelCoordsRenderManager::getTile(QRect rect)
{
	// One tile is what can be displayed in the viewport
	// at current scale factor levels
	// Check if aligned rectangle
	// if aligned rectangle fetch and return
	// else split into aligned rectangles
	// fetch and assemble
	// Note cache only holds images of the same scale level
	// so any request will be satisfied by atmost four rectangles
	int xVisiblePixels = canvasSize.width() * scaleFactors.first;
	int yVisiblePixels = canvasSize.height() * scaleFactors.second;

	QList<QRect> candidates;
	if(rect.top() % yVisiblePixels != 0) {
		QRect rect1(rect.left(),
					(rect.top() / yVisiblePixels) * yVisiblePixels,
					xVisiblePixels,
					yVisiblePixels);
		QRect rect2 = rect1;
		rect2.moveTop(rect1.top() + rect1.height());
		candidates.push_back(rect1);
		candidates.push_back(rect2);
	}
	else {
		candidates.push_back(rect);
	}

	for(int i=candidates.count(); i>0; i--) {
		QRect r = candidates.front();
		candidates.pop_front();
		if(r.left() % xVisiblePixels != 0) {
			QRect rect1((r.left() / xVisiblePixels) * xVisiblePixels,
						r.top(),
						xVisiblePixels,
						yVisiblePixels);
			QRect rect2 = rect1;
			rect2.moveLeft(rect1.left() + rect1.width());
			candidates.push_back(rect1);
			candidates.push_back(rect2);
		}
		else {
			candidates.push_back(r);
		}
	}

	QList<QRect> missing;
	foreach(QRect r, candidates) {
		if(imgCache.find(r) == imgCache.end())
			missing.push_back(r);
	}

	// If tiles are missing contruct them using the render function
	// add the new tiles to the cache
	// Construct image from tiles
	// return image
	if(!missing.isEmpty()) {
		foreach(QRect r, missing) {
			QVector<renderData> *ppd;
			QVector<QPolygonF> *polyLineSet;
			filterData(r, &ppd, &polyLineSet);
			auto i = renderImage(polyLineSet, ppd, r, viewportSize);
			imgCache.insert(r, i);

			delete ppd;
			delete polyLineSet;
		}
	}

	QImage *img = new QImage(viewportSize, QImage::Format_ARGB32_Premultiplied);
	img->fill(QColor(255,255,255));
	QPainter painter;
	{
		bool stat = painter.begin(img);
		Q_ASSERT(stat);
	}
	int xOffset, yOffset;
	xOffset = yOffset = 0;
	foreach(QRect r, candidates) {
		QRect cr = r & rect;
		QImage const * const i = imgCache[r];
		// translate from rect to image coordinates
		// rectangle uses canvas coords
		// image uses viewport coords
		qreal xScale = i->width() / static_cast<double>(r.width());
		qreal yScale = i->height() / static_cast<double>(r.height());
		QTransform t;
		t.scale(xScale, yScale);
		t.translate(r.left() * -1.0, r.top() * -1.0);
		QRect rectToCopy(t.mapRect(cr));
		QImage ii = i->copy(rectToCopy);
		painter.drawImage(xOffset, yOffset, ii);
		
		xOffset += rectToCopy.width();
		if(xOffset >= img->width()) {
			xOffset = 0;
			yOffset += rectToCopy.height();
		}
	}
	painter.end();

	// img is ready to send back
	emit tileGenerated(rect, img);
}

void ParallelCoordsRenderManager::filterData(
	QRectF visible_rect,
	QVector<renderData> **ppd_ptr,
	QVector<QPolygonF> **polyLineSet_ptr)
{
	auto compare = [](axis_view_data const& a, axis_view_data const& b)
	{
		return a.pos.x() < b.pos.x();	
	};

	// determine the axes just before the left and just after the right margins
	axis_view_data left = {-1, QRectF(), QPointF(visible_rect.left(), -1)};
	axis_view_data right = {-1, QRectF(), QPointF(visible_rect.right(), -1)};
	auto start_pos = qLowerBound((*axis_data).begin(), 
		(*axis_data).end(), left, compare);
	auto end_pos = qLowerBound(start_pos, (*axis_data).end(), right, compare);

	// adjust start_pos
	// start_pos might point to axis just inside the screen
	// we want to consider the axis before this one too
	if(start_pos != (*axis_data).begin())
		start_pos--;

	// adjust end_pos
	// end_pos might point to axis just off screen
	// we want to consider that axis to too
	if(end_pos != (*axis_data).end())
		end_pos++;

	QVector<renderData> *ppd = new QVector<renderData>;

	// Adjusting range here so that the min and max points appear on screen
	for(auto idx=start_pos; idx != end_pos; idx++) {
		renderData p = {
			idx->index, 									// axis index
			data->getRange(idx->index).first,				// min
			data->getRange(idx->index).second,				// max
			idx->pos.x(), 									// axis X
			idx->pos.y(), 									// axis Y
			idx->bounding_box.height()};					// axis height
			ppd->push_back(p);
	}

	// Process Points
	// Select -> Project -> construct polyline
	// drawing polylines can be faster than drawing line segments
	const int relevantAxisCnt = ppd->count();
	const int dataLength = data->length();
	auto *polyLineSet = new QVector<QPolygonF>;

	for(int i=0; i<dataLength; i++) {
		QPolygonF polyLine;
		for(int j=0; j<relevantAxisCnt; j++) {
			QPointF pt;
			const renderData &pp = (*ppd)[j];
			pt.setX(pp.axis_x);
			pt.setY(((*data)[i][pp.index] - pp.data_min) * (pp.axis_height) / 
				(pp.data_max - pp.data_min) + (pp.axis_y));
			polyLine << pt;
		}
		polyLineSet->push_back(polyLine);
	}

	*ppd_ptr = ppd;
	*polyLineSet_ptr = polyLineSet;
}

QImage* ParallelCoordsRenderManager::renderImage(
	QVector<QPolygonF> *polyLineSet, 
	QVector<renderData> *ppd, 
	// When anchor margins were under this function
	// it made sense to make viewport adjustments to 
	// appropriately draw the axis anchors. not needed
	// currently as this does not play well with the idea of tiling
// 	QRectF visible_rect, QSizeF parentViewportSize)
// {
// 	QSizeF viewportSize = parentViewportSize;
// 	viewportSize.setHeight(viewportSize.height() - 2.0 * axisAnchorMargin);

	QRectF visible_rect, QSizeF viewportSize)
{
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

				QPainter painter;
				{
					bool stat = painter.begin(img);
					Q_ASSERT(stat);
				}
				painter.drawImage(0, 0, *img1);
				painter.drawImage(0,viewportSize.height()/2.0, *img2);
				painter.end();

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

				QPainter painter;
				{
					bool stat = painter.begin(img);
					Q_ASSERT(stat);
				}
				painter.drawImage(0, 0, *img1);
				painter.drawImage(viewportSize.width()/2.0, 0, *img2);
				painter.end();

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

	QPainter painter;
	{
		bool stat = painter.begin(img);
		Q_ASSERT(stat);
	}
	QPen axisPen;
	axisPen.setWidthF(axisPenWidth);
	axisPen.setColor(QColor("#FF0000"));
	painter.setPen(axisPen);
	painter.drawLines(axisLines);
	painter.end();

	return img;

	// Axis margins don't play well with tiling
	// this has to be done in the final viewport
	// reverting back to the old ways

	// //draw axis anchors above and below appropriately
	// QVector<qreal> axisOffset;
	// for(int i=0; i<relevantAxisCnt; i++) {
	// 	QPointF pt = t.map(QPointF((*ppd)[i].axis_x, (*ppd)[i].axis_y));
	// 	axisOffset << 
	// 	(pt.x() * parentViewportSize.width() / viewportSize.width());
	// }

	// QImage *imgF = new QImage(parentViewportSize.toSize(), 
	// 						  QImage::Format_ARGB32_Premultiplied);
	// imgF->fill(QColor(255,255,255));
	// painter.begin(imgF);
	// axisPen.setWidthF(0);
	// axisPen.setColor(QColor("#FF0000"));
	// painter.setPen(axisPen);

	// QBrush b(painter.brush());
	// b.setColor(QColor("#FF0000"));
	// b.setStyle(Qt::SolidPattern);
	// painter.setBrush(b);

	// painter.drawImage(0, axisAnchorMargin, *img);
	// foreach(qreal x, axisOffset) {
	// 	painter.drawRect(x-9, 
	// 					 0, 
	// 					 axisAnchorMargin, 
	// 					 axisAnchorMargin);
	// 	painter.drawRect(x-9, 
	// 					 parentViewportSize.height()-axisAnchorMargin, 
	// 					 axisAnchorMargin, 
	// 					 axisAnchorMargin);
	// }
	// painter.end();

	// delete polyLineSet;
	// delete ppd;
	// delete img;

	// return imgF;
}