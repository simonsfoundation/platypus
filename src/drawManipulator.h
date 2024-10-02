#ifndef DRAWMANIPULATOR_H
#define DRAWMANIPULATOR_H

#include <editManipulator.h>

class DrawManipulator : public EditManipulator
{
	Q_OBJECT
public:
	DrawManipulator(QObject *parent = nullptr);
	~DrawManipulator();

    virtual void mouseHoverEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mousePressEvent(Viewer *viewer, QMouseEvent *event);
};

#endif
