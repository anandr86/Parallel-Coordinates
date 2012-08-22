
#include "ParallelCoordinates.h"
#include "QParallelCoordsWidget.h"
#include "ParallelCoordsRenderThread.h"
#include "ParallelCoordsRenderManager.h"
#include <memory>

QParallelCoordsWidget::QParallelCoordsWidget(QParallelCoordsData const *data_, 
											 QWidget *parent)
: QAbstractScrollArea(parent), data(data_)
{
	inter_axis_width = 0;
	axis_box_width = 0;
	scale_x = scale_y = 1;
	img = nullptr;
	currImgValid = false;
	isAxisSelected = false;
	axisMoveEngaged = false;
	axis_data = new QList<axis_view_data>();\
	selectedAxis = axis_data->end();
	rubberBand = nullptr;

	doLayout();

	renderManager = new ParallelCoordsRenderManager(
		canvas_size,
		qMakePair(scale_x, scale_y),
		viewport()->size(),
		axis_data,
		data);

	renderThread = new ParallelCoordsRenderThread(this, renderManager);
	renderThread->start();
}

QParallelCoordsWidget::~QParallelCoordsWidget()
{
	delete renderThread;
	delete axis_data;
}

void QParallelCoordsWidget::setup_scrollbar()
{
	QSizeF visible_extent;
	visible_extent.setWidth(canvas_size.width() * scale_x);
	visible_extent.setHeight(canvas_size.height() * scale_y);

	QSize scroll_size = (canvas_size - visible_extent).toSize();

	if(scroll_size.width() > 0) { // have to scroll horizontally
		int step = scroll_size.width()*0.3;
		horizontalScrollBar()->setPageStep(step > 1 ? step : 1);
		horizontalScrollBar()->setRange(0, scroll_size.width());
	}

	if(scroll_size.height() > 0) { // have to scroll vertically
		int step = scroll_size.height()*0.3;
		verticalScrollBar()->setPageStep(step > 1 ? step : 1);
		verticalScrollBar()->setRange(0, scroll_size.height());
	}
}

void QParallelCoordsWidget::doLayout()
{
	int y_extent = 0;
	int x_extent = 0;

	for(int i=0; i<data->axis_count(); i++) {
		auto range = data->getRange(i);
		y_extent = y_extent < range.second ? range.second : y_extent;
	}

	x_extent = data->axis_count() * axis_box_width + 
			   (data->axis_count()-1) * inter_axis_width;

	canvas_size = QSize(x_extent, y_extent);

	qreal x_offset = 0;
	for(int i=0; i<axis_data->count(); i++)
	{
		(*axis_data)[i].pos = QPointF(x_offset + axis_box_width/2.0, 0);
		(*axis_data)[i].bounding_box = QRectF(x_offset, 0, 
			x_offset + axis_box_width, canvas_size.height());
		x_offset += inter_axis_width + axis_box_width;
	}
}

void QParallelCoordsWidget::updateLayout()
{
	updateView(true);
}

qreal QParallelCoordsWidget::getXScale() const
{
	return scale_x;
}

void QParallelCoordsWidget::setXScale(qreal scale)
{
	currImgValid = false;
	scale_x = scale;
	emit scaleFactorsChange(qMakePair(scale_x, scale_y));
	updateView();
}

qreal QParallelCoordsWidget::getYScale() const
{
	return scale_y;
}

void QParallelCoordsWidget::setYScale(qreal scale)
{
	currImgValid = false;
	scale_y = scale;
	emit scaleFactorsChange(qMakePair(scale_x, scale_y));
	updateView();
}

void QParallelCoordsWidget::setXScale(int scale)
{
	setXScale(scale/100.0);
}

void QParallelCoordsWidget::setYScale(int scale)
{
	setYScale(scale/100.0);
}

void QParallelCoordsWidget::resizeEvent(QResizeEvent *event)
{
	Q_UNUSED(event);
	emit viewportSizeChange(viewport()->size());
	updateView();
}

void QParallelCoordsWidget::scrollContentsBy(int dx, int dy)
{
	Q_UNUSED(dx);
	Q_UNUSED(dy);
	updateView();
}

int QParallelCoordsWidget::getInterAxisWidth() const
{
	return inter_axis_width;
}

void QParallelCoordsWidget::setInterAxisWidth(int w)
{
	inter_axis_width = w;
	updateView(true);
}

int QParallelCoordsWidget::getAxisBoxWidth() const
{
	return axis_box_width;
}

void QParallelCoordsWidget::setAxisBoxWidth(int w)
{
	axis_box_width = w;
	updateView(true);
}

void QParallelCoordsWidget::updateView(bool doLayout_)
{
	isAxisSelected = false;
	selectedAxis = axis_data->end();
	if(axis_data->count() != data->axis_count()) {
		axis_data->clear();

		for(int i=0; i<data->axis_count(); i++) {
			axis_view_data avd = {i, QRectF(), QPointF()};
			axis_data->push_back(avd);
		}
		doLayout_ = true;
	}

	if(doLayout_) {
		doLayout();
		emit axisDataChange();
		emit canvasSizeChange(canvas_size);
	}

	setup_scrollbar();
	viewport()->update();
}

void QParallelCoordsWidget::mousePressEvent(QMouseEvent *event)
{
	// We need a valid current image to process this event
	// nothing on display nothing to process
	if(!currImgValid)
		return;

	// need more than one axis to make some sense
	if(axis_data->count() <= 1)
		return;

	// user is going to use the rubber band now
	// no need to deselect
	if(isAxisSelected && curveMode)
		return;

	QSize viewportSize = viewport()->size();
	QTransform t;
	t.scale(static_cast<double>(viewportSize.width())/curr_rect.width(), 
		static_cast<double>(viewportSize.height())/curr_rect.height());
	t.translate(curr_rect.left()*-1.0, curr_rect.top()*-1.0);

	// reduce 5 from x 
	// this is to accomodate for error in click
	// If we are off by 5 to the right (max) then we still get the right pos
	// from qLowerBound after this adjust
	// If we are spot on or to the left this must not affect qLowerBound anyway
	QPoint pt;
	{
		QPoint ept = event->pos();
		ept.setX(ept.x() - t.mapRect(QRect(0, 0, 5, 5)).width());
		pt = t.inverted().map(ept);
	}

	auto compare = [](axis_view_data const& a, axis_view_data const& b)
	{
		return a.pos.x() < b.pos.x();	
	};

	// determine the axes just before the left and just after the right margins
	axis_view_data ptPos = {-1, QRectF(), QPointF(pt.x(), -1)};
	auto pos = qLowerBound(axis_data->begin(), 
		axis_data->end(), ptPos, compare);
	auto axis = axis_data->end();

	if(pos == axis_data->end() || (pos->pos.x() - pt.x() > 10)) {
		isAxisSelected = false;
		emit axisSelected(-1);
		viewport()->repaint();
		return;
	}

	isAxisSelected = true;
	selectedAxis = pos;
 
	emit axisSelected(pos->index);
	viewport()->repaint();
}

void QParallelCoordsWidget::setCurveMode(bool state)
{
	curveMode = state;
}

bool QParallelCoordsWidget::getCurveMode()
{
	return curveMode;
}

void QParallelCoordsWidget::mouseMoveEvent(QMouseEvent *event)
{
	if(!isAxisSelected)
		return;
	if(!curveMode) {
		axisMoveEngaged = true;
		axisMovePos = event->pos();
		viewport()->repaint();
	}
	else {
		if(!rubberBand) {
			rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
			rubberBand->move(event->pos());
			rubberBand->show();
		}
		else {
			QPoint opos = rubberBand->pos();
			QPoint npos = event->pos();

			QPoint pos(qMin(opos.x(), npos.x()), qMin(opos.y(), npos.y()));
			QSize s(qAbs(npos.x() - opos.x()), qAbs(npos.y() - opos.y()));
			rubberBand->setGeometry(QRect(pos, s));
		}
	}
}

void QParallelCoordsWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(axisMoveEngaged) {
		QSize viewportSize = viewport()->size();
		QTransform t;
		t.scale(static_cast<double>(viewportSize.width())/curr_rect.width(), 
			static_cast<double>(viewportSize.height())/curr_rect.height());
		t.translate(curr_rect.left()*-1.0, curr_rect.top()*-1.0);

		selectedAxis->pos.setX(t.inverted().map(event->pos()).x());
		qSort(axis_data->begin(), axis_data->end(), 
			[](axis_view_data a, axis_view_data b) 
			{return a.pos.x() < b.pos.x();});

		axisMoveEngaged = false;
		isAxisSelected = false;

		emit axisDataChange();
		viewport()->update();
	}
	else if(rubberBand) {
		isAxisSelected = false;
		rubberBand->hide();
		delete rubberBand;
		rubberBand = nullptr;
	}
}

void QParallelCoordsWidget::paintEvent(QPaintEvent *event)
{
	/* 
	 * Check if we have a new image to draw. If so draw and return
	 * If we have a valid current image then draw that on screen
	 * If an axis was selected then update visuals and don't disable scrll brs
	*/
	Q_UNUSED(event);
	if(data->axis_count() <= 0) return;

	if(img != nullptr) {
		QPainter painter;
		{
			bool stat = painter.begin(viewport());
			Q_ASSERT(stat);
		}
		painter.drawImage(0, 0, *img);
		painter.end();
		curr_img.swap(*img);
		curr_rect = img_rect;
		img_rect = QRect(0, 0, 0, 0);

		currImgValid = true;

		horizontalScrollBar()->setEnabled(true);
		verticalScrollBar()->setEnabled(true);

		delete img;
		img = nullptr;
		return;
	}

	if(currImgValid) {
		if(isAxisSelected) {
			QTransform t;
			QSizeF viewportSize = viewport()->size();
			t.scale(viewportSize.width()/curr_rect.width(),
					viewportSize.height()/curr_rect.height());
			t.translate(curr_rect.left() * -1, curr_rect.top() * -1);
			QPoint screenPos = t.map(QPoint(selectedAxis->pos.x(), 
											selectedAxis->pos.y()));

			QRect before = QRect(0, 0, 
				viewportSize.width(), viewportSize.height());
			QRect after = before;
			before.setRight(screenPos.x()-2);
			after.setLeft(screenPos.x()+2);

			QImage img(viewportSize.toSize(), 
				QImage::Format_ARGB32_Premultiplied);
			QPainter painter;
			painter.begin(&img);
			painter.drawImage(0, 0, curr_img);
			QColor c(255,255,255, 95);
			painter.fillRect(before, c);
			painter.fillRect(after, c);

			if(axisMoveEngaged) {
				QPen p = painter.pen();
				p.setWidthF(2);
				p.setColor(QColor(0, 0, 255));
				painter.setPen(p);
				painter.drawLine(axisMovePos.x(), 0, 
					axisMovePos.x(), viewportSize.height());
			}
			painter.end();

			painter.begin(viewport());
			painter.drawImage(0, 0, img);
			painter.end();
			return;
		}
		else {
			QPainter painter;
			painter.begin(viewport());
			painter.drawImage(0, 0, curr_img);
			painter.end();
		}
	}

	QRect r(horizontalScrollBar()->value(),
			verticalScrollBar()->value(),
			canvas_size.width() * scale_x,
			canvas_size.height() * scale_y);

	emit requestTile(r);
	horizontalScrollBar()->setEnabled(false);
	verticalScrollBar()->setEnabled(false);
}

void QParallelCoordsWidget::renderTile(QRect r, QImage *img_)
{ 
	img = img_;
	img_rect = r;
	viewport()->update();
}