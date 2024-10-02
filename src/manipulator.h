#ifndef MANIPULATOR_H
#define MANIPULATOR_H

#include <QtCore/QObject>

class Viewer;
class QMouseEvent;
class QKeyEvent;
class QPainter;
class ImageSource;

class Manipulator : public QObject
{
	Q_OBJECT
public:
	Manipulator(QObject *parent = nullptr);
	~Manipulator();

	virtual ImageSource *source() const;
    
    virtual bool receiveEventsWhenDisabled() const;

    virtual void mouseHoverEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mousePressEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mouseMoveEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mouseReleaseEvent(Viewer *viewer, QMouseEvent *event);
    virtual void keyPressEvent(Viewer *viewer, QKeyEvent *event);
    virtual void keyReleaseEvent(Viewer *viewer, QKeyEvent *event);
    virtual void draw(const Viewer *viewer, QPainter &p) const;

	virtual bool canDoCommand(const QString &cmdName) const;
	virtual void doCommand(const QString &cmdName);
    
    virtual void beginParameterChange(const QString &key);
    virtual void endParameterChange(const QString &key);

public:
	static bool pointOnLine(const QPointF &p0, const QPointF &p1, const QPointF &mouse_pt, float tolerance, float *t = nullptr);

Q_SIGNALS:

public slots:

protected:
	virtual void cancel(Viewer *viewer);
    virtual void nudge(Viewer *viewer, const QPointF &delta);

private:
};

#endif
