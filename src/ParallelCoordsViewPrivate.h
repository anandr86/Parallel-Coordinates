
#ifndef __PARALLELCOORDSVIEWPRIVATE__
#define __PARALLELCOORDSVIEWPRIVATE__

#include "ParallelCoordinates.h"

struct axis_view_data {
	int index;
	QRectF bounding_box;
	QPointF pos;
};

struct renderData {
	int index;
	qreal data_min;
	qreal data_max;
	qreal axis_x;
	qreal axis_y;
	qreal axis_height;
};

#endif