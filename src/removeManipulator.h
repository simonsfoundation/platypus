#ifndef REMOVEMANIPULATOR_H
#define REMOVEMANIPULATOR_H

#include <editManipulator.h>
#include <QtCore/QRect>

class RemoveManipulator : public EditManipulator
{
	Q_OBJECT
public:
	RemoveManipulator(QObject *parent = nullptr);
	~RemoveManipulator();

	virtual Polygon::Type polygonType() const;

	virtual ImageSource *source() const;
    virtual bool receiveEventsWhenDisabled() const;

    virtual void draw(const Viewer *viewer, QPainter &p) const;
    virtual void mousePressEvent(Viewer *viewer, QMouseEvent *event);

	virtual bool canDoCommand(const QString &cmdName) const;
	virtual void doCommand(const QString &cmdName);

    virtual void beginParameterChange(const QString &key);
    virtual void endParameterChange(const QString &key);

protected:
    virtual bool canEdit() const;

private:
};

#endif
