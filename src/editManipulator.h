#ifndef EDITMANIPULATOR_H
#define EDITMANIPULATOR_H

#include <polygonManipulator.h>

class EditManipulator : public PolygonManipulator
{
	Q_OBJECT
public:
	EditManipulator(QObject *parent = nullptr);
	~EditManipulator();

    virtual void mouseHoverEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mousePressEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mouseMoveEvent(Viewer *viewer, QMouseEvent *event);
    virtual void mouseReleaseEvent(Viewer *viewer, QMouseEvent *event);
    virtual void keyPressEvent(Viewer *viewer, QKeyEvent *event);
    virtual void keyReleaseEvent(Viewer *viewer, QKeyEvent *event);
    virtual void draw(const Viewer *viewer, QPainter &p) const;

	virtual bool canDoCommand(const QString &cmdName) const;
	virtual void doCommand(const QString &cmdName);

protected:
    enum Mode
    {
        kMode_None,
        kMode_Draw,
        kMode_Drag,
        kMode_Insert,
        kMode_Resize,
        kMode_Rotate,
        kMode_Select
    };

	void createPolygon(Viewer *viewer, QMouseEvent *event);
	QPoint findAnchor() const;

	virtual void cancel(Viewer *viewer);
    virtual void nudge(Viewer *viewer, const QPointF &delta);
    virtual bool canEdit() const;

    void beginSelect(Viewer *viewer, QMouseEvent *event);
    void doSelect(Viewer *viewer, QMouseEvent *event);
    void endSelect(Viewer *viewer, QMouseEvent *event);

	// PolygonManipulator
	virtual Polygon::Type polygonType() const;

private:
    Mode m_mode;
    PolygonPointer m_polygon;       // when creating

	struct Edit
	{
		PolygonPointer poly;
		Polygon::PointList backup;
		QVector<int> selection;
        Polygon::Handle handle;
        Polygon::Point point;

		Edit(const PolygonPointer &p) : poly(p), backup(p->points()), handle(Polygon::kHandle_None) {}

		Polygon::PointList move(const QPoint &delta, int modifiers) const;
        Polygon::PointList rotate(const QPoint &anchor, float angle) const;
	};
	QList<Edit> m_edits;
    float m_t;
};

#endif
