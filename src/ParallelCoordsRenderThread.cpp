
#include "ParallelCoordinates.h"
#include "ParallelCoordsRenderThread.h"
#include "ParallelCoordsViewPrivate.h"

ParallelCoordsRenderThread::ParallelCoordsRenderThread(
	QObject *parent,
	ParallelCoordsRenderManager *renderManager_)
: QThread(parent)
{
	renderManager = renderManager_;
	renderManager->moveToThread(this);

	qRegisterMetaType<QList<axis_view_data>>("QList<axis_view_data>");
	qRegisterMetaType<QPair<qreal, qreal>>("QPair<qreal, qreal>");

	connect(parent, SIGNAL(requestTile(QRect)), 
			renderManager, SLOT(getTile(QRect)));
	connect(renderManager, SIGNAL(tileGenerated(QRect, QImage*)),
			parent, SLOT(renderTile(QRect, QImage*)));
	connect(parent, SIGNAL(scaleFactorsChange(QPair<qreal, qreal>)),
			renderManager, SLOT(scaleFactorsChange(QPair<qreal, qreal>)));
	connect(parent, SIGNAL(viewportSizeChange(QSize)),
			renderManager, SLOT(viewportSizeChange(QSize)));
	connect(parent, SIGNAL(canvasSizeChange(QSize)),
			renderManager, SLOT(canvasSizeChange(QSize)));
	connect(parent, SIGNAL(axisDataChange()),
			renderManager, SLOT(axisDataChange()));
}

ParallelCoordsRenderThread::~ParallelCoordsRenderThread()
{
	delete renderManager;
}