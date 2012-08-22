
#ifndef __PARALLELCOORDSVISUALIZER_H__
#define __PARALLELCOORDSVISUALIZER_H__

#include "ParallelCoordinates.h"
#include "QParallelCoordsData.h"
#include "QParallelCoordsWidget.h"
#include <QMainWindow>

class ParallelCoordsVisualizer : public QWidget
{
	Q_OBJECT

public:
	ParallelCoordsVisualizer(QWidget *parent);
	~ParallelCoordsVisualizer();

private:
	void init_components();
	QParallelCoordsData *data;
	QLabel *infoLabel;
	QParallelCoordsWidget *coord_wd;

private slots:
	void loadFile();
	void axisSelected(int idx);
	void setCurveMode(int state);
};

#endif