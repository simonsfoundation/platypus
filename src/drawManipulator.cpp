#include <drawManipulator.h>
#include <project.h>
#include <polygon.h>
#include <viewer.h>
#include <undoManager.h>
#include <polygonCommands.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>

DrawManipulator::DrawManipulator(QObject *parent) : EditManipulator(parent)
{
}

DrawManipulator::~DrawManipulator()
{
}

void DrawManipulator::mouseHoverEvent(Viewer *viewer, QMouseEvent *event)
{
	// bypass Edit hover
	Manipulator::mouseHoverEvent(viewer, event);
}

void DrawManipulator::mousePressEvent(Viewer *viewer, QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
	{
		createPolygon(viewer, event);
	}
	if (!event->isAccepted())
		Manipulator::mousePressEvent(viewer, event);
}
