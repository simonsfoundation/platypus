#ifndef MASKMANIPULATOR_H
#define MASKMANIPULATOR_H

#include <editManipulator.h>

class MaskManipulator : public EditManipulator
{
	Q_OBJECT
public:
	MaskManipulator(QObject *parent = nullptr);
	~MaskManipulator();

	virtual Polygon::Type polygonType() const;

protected:
    virtual void drawMask(const Viewer *viewer, QPainter &p) const;

private:
};

#endif
