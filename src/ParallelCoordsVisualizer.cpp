
#include "ParallelCoordinates.h"
#include "ParallelCoordsVisualizer.h"
#include "QParallelCoordsWidget.h"
#include "QParallelCoordsData.h"
#include <algorithm>

ParallelCoordsVisualizer::ParallelCoordsVisualizer(QWidget *parent)
: QWidget(parent)
{
	setWindowTitle("Parallel Coordinates Visualizer");
	init_components();
}

ParallelCoordsVisualizer::~ParallelCoordsVisualizer()
{

}

void ParallelCoordsVisualizer::loadFile()
{
	QString fname = QFileDialog::getOpenFileName(this, tr("Select Input file"), "", "CSV File file (*.csv)");
	QFile inpFile(fname);

	if(!inpFile.exists()) return;

	inpFile.open(QIODevice::ReadOnly);
	QTextStream txtStrm(&inpFile);
	QString inp = txtStrm.readLine();
	QStringList axisNames = inp.split(",", QString::SkipEmptyParts);
	int axisCnt = axisNames.count();

	data->setAxisCount(axisCnt);
	{
		int idx = 0;
		foreach(QString name, axisNames) {
			data->setAxisName(idx++, name);
		}
		inp = txtStrm.readLine();
	}

	QList<QVector<qreal>> pts;
	while(!inp.isEmpty()) {
		QVector<qreal> pt(axisCnt);
		QStringList inpList = inp.split(",", QString::SkipEmptyParts);
		std::transform(inpList.begin(), inpList.begin()+axisCnt, pt.begin(), [](QString s){return s.toDouble();});
		pts.push_back(pt);
		inp = txtStrm.readLine();
	}
	data->addPoints(pts);
	inpFile.close();
}

void ParallelCoordsVisualizer::axisSelected(int idx)
{
	if(idx == -1) {
		infoLabel->setText("Select an axis to view the information on this bar");
		return;
	}
	infoLabel->setText(QString("Selected: %1").arg(data->getAxisName(idx)));
}

void ParallelCoordsVisualizer::setCurveMode(int state)
{
	if(state == 0) {
		coord_wd->setCurveMode(false);
	}
	else {
		coord_wd->setCurveMode(true);
	}
}

void ParallelCoordsVisualizer::init_components()
{
	data = new QParallelCoordsData(this);
	QGridLayout *layout = new QGridLayout(this);

	coord_wd = new QParallelCoordsWidget(data);
	connect(data, SIGNAL(dataChanged(bool)), coord_wd, SLOT(updateView(bool)));

	QWidget *wd = new QPushButton("Load File");
	layout->addWidget(wd, 0, 0);
	connect(wd, SIGNAL(clicked()), this, SLOT(loadFile()));
	
	wd = new QLabel("Inter-Axis span");
	layout->addWidget(wd, 0, 1);

	wd = new QSpinBox();
	layout->addWidget(wd, 0, 2);
	static_cast<QSpinBox*>(wd)->setRange(50, 1000);
	coord_wd->setInterAxisWidth(50);
	static_cast<QSpinBox*>(wd)->setValue(coord_wd->getInterAxisWidth());
	connect(wd, SIGNAL(valueChanged(int)), coord_wd, SLOT(setInterAxisWidth(int)));

	wd = new QLabel("Axis boundary width");
	layout->addWidget(wd, 0, 3);

	wd = new QSpinBox();
	layout->addWidget(wd, 0, 4);
	static_cast<QSpinBox*>(wd)->setRange(20, 100);
	coord_wd->setAxisBoxWidth(20);
	static_cast<QSpinBox*>(wd)->setValue(coord_wd->getAxisBoxWidth());
	connect(wd, SIGNAL(valueChanged(int)), coord_wd, SLOT(setAxisBoxWidth(int)));

	wd = new QLabel("X Zoom");
	layout->addWidget(wd, 0, 5);

	wd = new QSlider(Qt::Horizontal);
	layout->addWidget(wd, 0, 6);
	static_cast<QSlider*>(wd)->setRange(1,200);
	static_cast<QSlider*>(wd)->setValue(100);
	static_cast<QSlider*>(wd)->setSingleStep(10);
	static_cast<QSlider*>(wd)->setPageStep(20);
	static_cast<QSlider*>(wd)->setTracking(false);
	connect(wd, SIGNAL(valueChanged(int)), coord_wd, SLOT(setXScale(int)));

	wd = new QLabel("Y Zoom");
	layout->addWidget(wd, 0, 7);

	wd = new QSlider(Qt::Horizontal);
	layout->addWidget(wd, 0, 8);
	static_cast<QSlider*>(wd)->setRange(1,200);
	static_cast<QSlider*>(wd)->setValue(100);
	static_cast<QSlider*>(wd)->setSingleStep(10);
	static_cast<QSlider*>(wd)->setPageStep(20);
	static_cast<QSlider*>(wd)->setTracking(false);
	connect(wd, SIGNAL(valueChanged(int)), coord_wd, SLOT(setYScale(int)));

	wd = new QPushButton("Layout");
	layout->addWidget(wd, 0, 9);
	connect(wd, SIGNAL(clicked()), coord_wd, SLOT(updateLayout()));

	wd = new QCheckBox("Curve axis");
	layout->addWidget(wd, 0, 10);
	connect(wd, SIGNAL(stateChanged(int)), this, SLOT(setCurveMode(int)));

	infoLabel = new QLabel("Select an axis to view the information on this bar");
	layout->addWidget(infoLabel, 1, 0);
	connect(coord_wd, SIGNAL(axisSelected(int)), this, SLOT(axisSelected(int)));

	layout->addWidget(coord_wd, 1, 1, 1, -1);
}

int main(int argc, char** argv)
{
	QApplication app(argc, argv);

	ParallelCoordsVisualizer *wnd = new ParallelCoordsVisualizer(0);
	wnd->show();

	return app.exec();
}