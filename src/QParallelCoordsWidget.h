
#ifndef __QPARALLELCOORDSWIDGET_H__
#define __QPARALLELCOORDSWIDGET_H__

#include "ParallelCoordinates.h"
#include "QParallelCoordsData.h"
#include "ParallelCoordsViewPrivate.h"
#include "ParallelCoordsRenderThread.h"

class QParallelCoordsWidget : public QAbstractScrollArea
{
	Q_OBJECT
	Q_PROPERTY(int inter_axis_width READ getInterAxisWidth WRITE setInterAxisWidth)
	Q_PROPERTY(int axis_box_width READ getAxisBoxWidth WRITE setAxisBoxWidth)
	Q_PROPERTY(qreal scale_x READ getXScale WRITE setXScale)
	Q_PROPERTY(qreal scale_y READ getYScale WRITE setYScale)
	Q_PROPERTY(bool curveMode READ getCurveMode WRITE setCurveMode);

public:
	QParallelCoordsWidget(QParallelCoordsData const *data_, QWidget *parent = 0);
	~QParallelCoordsWidget();

	int getInterAxisWidth() const;
	int getAxisBoxWidth() const;
	qreal getXScale() const;
	qreal getYScale() const;
	void setCurveMode(bool state);
	bool getCurveMode();

signals:
	void requestTile(QRect r);
	void scaleFactorsChange(QPair<qreal, qreal> f);
	void viewportSizeChange(QSize viewportSize);
	void canvasSizeChange(QSize canvasSize);
	void axisDataChange();
	void axisSelected(int idx);

public slots:
	void setXScale(int scale);
	void setYScale(int scale);
	void setInterAxisWidth(int w);
	void setAxisBoxWidth(int w);
	void setXScale(qreal scale);
	void setYScale(qreal scale);
	void renderTile(QRect r, QImage *img);
	void updateView(bool doLayout_ = false);
	void updateLayout();

private:
	ParallelCoordsRenderThread *renderThread;
	ParallelCoordsRenderManager *renderManager;
	QParallelCoordsData const *data;
	QSize canvas_size;
	qreal inter_axis_width, axis_box_width;
	qreal scale_x, scale_y;
	QImage *img;
	QRect img_rect;
	QImage curr_img;
	QRect curr_rect;
	bool currImgValid;
	bool isAxisSelected;
	bool axisMoveEngaged;
	bool curveMode;
	QPoint axisMovePos;
	QRubberBand *rubberBand;

	QList<axis_view_data>::iterator selectedAxis; 

	QList<axis_view_data> *axis_data;
	void doLayout();
	void setup_scrollbar();

protected:
	void paintEvent(QPaintEvent *event);
	void resizeEvent(QResizeEvent *event);
	void scrollContentsBy(int dx, int dy);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
};
#endif