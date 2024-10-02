#include <removeManipulator.h>
#include <removeSource.h>
#include <imageManager.h>
#include <project.h>
#include <polygonCommands.h>
#include <viewer.h>
#include <undoManager.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <cmath>

static bool s_paramChange = false;

RemoveManipulator::RemoveManipulator(QObject *parent) : EditManipulator(parent)
{
}

RemoveManipulator::~RemoveManipulator()
{
}

bool RemoveManipulator::receiveEventsWhenDisabled() const
{
    return true;
}

Polygon::Type RemoveManipulator::polygonType() const
{
    return Polygon::OUTPUT;
}

bool RemoveManipulator::canEdit() const
{
    return false;
}

ImageSource *RemoveManipulator::source() const
{
	return ImageManager::get().removeSource();
}

void RemoveManipulator::draw(const Viewer *viewer, QPainter &p) const
{
    if (!s_paramChange)
        PolygonManipulator::draw(viewer, p);
}

bool RemoveManipulator::canDoCommand(const QString &cmdName) const
{
    if (cmdName == "selectAll")
        return EditManipulator::canDoCommand(cmdName);
	return Manipulator::canDoCommand(cmdName);
}

void RemoveManipulator::doCommand(const QString &cmdName)
{
	EditManipulator::doCommand(cmdName);
}

void RemoveManipulator::beginParameterChange(const QString &key)
{
    s_paramChange = true;
}

void RemoveManipulator::endParameterChange(const QString &key)
{
    s_paramChange = false;
}

void RemoveManipulator::mousePressEvent(Viewer *viewer, QMouseEvent *event)
{
    if (event->modifiers() == Qt::AltModifier)
    {
        PolygonPointer poly = findPolygon(viewer, event->pos());
        if (poly && !poly->isSelected())
        {
            int black = 0;
            int gamma = 0;
            int white = 0;
            int count = 0;
            for (auto p : selection())
            {
                int bvalue = p->value("black").toInt();
                int gvalue = p->value("gamma").toInt();
                int wvalue = p->value("white").toInt();
                if (count == 0)
                {
                    black = bvalue;
                    gamma = gvalue;
                    white = wvalue;
                    count++;
                }
                else
                {
                    if (bvalue != black || gvalue != gamma || wvalue != white)
                        count++;
                }
            }
            if (count == 1)
            {
                UndoManager::instance()->beginMacro("Copy Levels");
                UndoManager::instance()->push(new EditPolygonCommand(poly, "black", black));
                UndoManager::instance()->push(new EditPolygonCommand(poly, "gamma", gamma));
                UndoManager::instance()->push(new EditPolygonCommand(poly, "white", white));
                UndoManager::instance()->endMacro();
            }
        }
    }
    else
        EditManipulator::mousePressEvent(viewer, event);
}


