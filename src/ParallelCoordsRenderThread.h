#ifndef __PARELLELCOORDSRENDERTHREAD_H__
#define __PARELLELCOORDSRENDERTHREAD_H__

#include "ParallelCoordinates.h"
#include "ParallelCoordsRenderThread.h"
#include "ParallelCoordsRenderManager.h"

class ParallelCoordsRenderThread : public QThread
{
public:
	ParallelCoordsRenderThread(
		QObject *parent,
		ParallelCoordsRenderManager *renderManager);
	~ParallelCoordsRenderThread();

private:
	ParallelCoordsRenderManager *renderManager;
};

#endif