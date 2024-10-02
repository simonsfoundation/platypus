#ifndef POLYGOMANIPULATOR_H
#define POLYGOMANIPULATOR_H

#include <manipulator.h>
#include <polygon.h>

class PolygonManipulator : public Manipulator
{
	Q_OBJECT
public:
	PolygonManipulator(QObject *parent = nullptr);

protected:
	virtual Polygon::Type polygonType() const = 0;
    virtual bool canEdit() const = 0;

	virtual void draw(const Viewer *viewer, QPainter &p) const;
    virtual void drawMask(const Viewer *viewer, QPainter &p) const;

	PolygonList selection() const;

	PolygonPointer findPolygon(const Viewer *viewer, const QPoint &mouse) const;
	int findHandle(const Viewer *viewer, const Polygon &poly, const QPoint &mouse, int modifiers, Polygon::Handle &handle) const;
	int findEdge(const Viewer *viewer, const Polygon &poly, const QPoint &mouse, int modifiers) const;
    void drawPolygon(const Viewer *viewer, QPainter &p, const Polygon &poly) const;
    bool setHover(const PolygonPointer &p, int index = -1, Polygon::Handle handle = Polygon::kHandle_None);

    const PolygonPointer &hoverPolygon() const { return m_hoverPolygon; }
    int hoverIndex() const { return m_hoverIndex; }
    Polygon::Handle hoverHandle() const { return m_hoverHandle; }

	float pointOnShape(const PolygonPointer &p, const QPoint &pos, float dist) const;
	PolygonPointer pointOnShape(const QPoint &pos, float dist, float *t = nullptr) const;

private:
    PolygonPointer m_hoverPolygon;
    int m_hoverIndex;
    Polygon::Handle m_hoverHandle;
};

#endif
