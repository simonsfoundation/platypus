#include <polygonManipulator.h>
#include <project.h>
#include <viewer.h>
#include <QtGui/QPainter>

PolygonManipulator::PolygonManipulator(QObject *parent) : Manipulator(parent)
{
}

PolygonList PolygonManipulator::selection() const
{
	return Project::activeProject()->selection(polygonType());
}

PolygonPointer PolygonManipulator::findPolygon(const Viewer *viewer, const QPoint &mouse) const
{
	const QList<PolygonPointer> &polygons = Project::activeProject()->polygons();
    Polygon::Type type = polygonType();
	for (auto polygon : polygons)
	{
		if (polygon->type() == type)
		{
			if (polygon->path(viewer->getImageSize()).contains(viewer->viewToSource(mouse).toPoint()))
				return polygon;
		}
	}
	return PolygonPointer();
}

int PolygonManipulator::findHandle(const Viewer *viewer, const Polygon &poly, const QPoint &mouse, int modifiers, Polygon::Handle &handle) const
{
    handle = Polygon::kHandle_None;
    int index = -1;
    const auto &points = poly.points();
	for (int i = 0; i < points.size() && index < 0; i++)
	{
        const Polygon::Point &p = points[i];
        QPointF knot = viewer->sourceToView(p.knot);
        if (poly.shapeType() == Polygon::POLYGON)
        {
            if (viewer->hitPoint(knot, mouse))
            {
                handle = Polygon::kHandle_Knot;
                index = i;
            }
        }
        else
        {
            QPointF tan_in = viewer->sourceToView(p.tan_in + p.knot);
            QPointF tan_out = viewer->sourceToView(p.tan_out + p.knot);

            if (viewer->hitPoint(knot, mouse) && !(modifiers & Qt::AltModifier))
            {
                handle = Polygon::kHandle_Knot;
                index = i;
            }
            else if (viewer->hitPoint(tan_out, mouse))
            {
                handle = Polygon::kHandle_OutTangent;
                index = i;
            }
            else if (viewer->hitPoint(tan_in, mouse))
            {
                handle = Polygon::kHandle_InTangent;
                index = i;
            }
        }
	}
	return index;
}

int PolygonManipulator::findEdge(const Viewer *viewer, const Polygon &poly, const QPoint &mouse, int modifiers) const
{
    if (modifiers & Qt::AltModifier)
    {
        float handleSize = viewer->handleSize();
        const auto &points = poly.points();
        for (int i = 0; i < points.size(); i++)
        {
            QPointF p0 = viewer->sourceToView(points[i].knot);
            QPointF p1 = viewer->sourceToView(i == points.size() - 1 ? points[0].knot : points[i + 1].knot);

            if (Manipulator::pointOnLine(p0, p1, mouse, handleSize / 2.0f))
                return i;
        }
    }
	return -1;
}

void PolygonManipulator::drawMask(const Viewer *viewer, QPainter &p) const
{
    // draw mask
    QColor color(255, 0, 0, 64);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
	p.drawPath(Project::activeProject()->maskPath(viewer->getImageSize()));
}

void PolygonManipulator::draw(const Viewer *viewer, QPainter &p) const
{
    drawMask(viewer, p);

	// draw polygons
	const Project *project = Project::activeProject();
	if (project)
	{
        // draw unselected polygons, then selected polygonsr
        Polygon::Type type = polygonType();
		const auto &polygons = project->polygons();
		for (auto polygon : polygons)
		{
			if (polygon->type() == type && !polygon->isSelected())
				drawPolygon(viewer, p, *polygon);
		}
		for (auto polygon : polygons)
		{
			if (polygon->type() == type && polygon->isSelected())
				drawPolygon(viewer, p, *polygon);
		}
	}
}

void PolygonManipulator::drawPolygon(const Viewer *viewer, QPainter &p, const Polygon &poly) const
{
    QColor polygonColor = poly.isSelected() ? Qt::green : poly.color();
    QColor fill(polygonColor.red(), polygonColor.green(), polygonColor.blue(), 64);

    QPen pen(polygonColor, 2.0f);
    pen.setCosmetic(true);
    p.setPen(pen);
    if (poly.type() == Polygon::OUTPUT)
        p.setBrush(Qt::NoBrush);
    else
        p.setBrush(QBrush(fill));

    p.drawPath(poly.path(viewer->getImageSize()));

	// draw handles
	if (poly.isSelected() && canEdit())
	{
		//pen.setWidthF(1.0f);
	    p.setPen(pen);
		p.setBrush(Qt::NoBrush);

		float z = viewer->getZoom();
		float handleSize = viewer->handleSize() / z;

		const auto &points = poly.points();
        for (int i = 0; i < points.size(); i++)
		{
            const Polygon::Point &point = points[i];
			// draw center knot
			const QPointF &pt = point.knot;
            pen.setColor((&poly == m_hoverPolygon && i == m_hoverIndex && m_hoverHandle == Polygon::kHandle_Knot) ? Qt::red : polygonColor);
            p.setPen(pen);

			p.drawRect(QRectF(pt.x() - handleSize / 2.0f, pt.y() - handleSize / 2.0f, handleSize, handleSize));

			// draw tangent handles
            if (poly.shapeType() == Polygon::BEZIER)
            {
                if (!point.tan_in.isNull())
                {
                    pen.setColor((&poly == m_hoverPolygon && i == m_hoverIndex && m_hoverHandle == Polygon::kHandle_InTangent) ? Qt::red : polygonColor);
                    p.setPen(pen);

                    QPointF pt = point.knot + point.tan_in;
                    p.drawRect(QRectF(pt.x() - handleSize / 2.0f, pt.y() - handleSize / 2.0f, handleSize, handleSize));
                    p.drawLine(point.knot, pt);
                }
                if (!point.tan_out.isNull())
                {
                    pen.setColor((&poly == m_hoverPolygon && i == m_hoverIndex && m_hoverHandle == Polygon::kHandle_OutTangent) ? Qt::red : polygonColor);
                    p.setPen(pen);

                    QPointF pt = point.knot + point.tan_out;
                    p.drawRect(QRectF(pt.x() - handleSize / 2.0f, pt.y() - handleSize / 2.0f, handleSize, handleSize));
                    p.drawLine(point.knot, pt);
                }
            }
		}
    }
}

bool PolygonManipulator::setHover(const PolygonPointer &p, int index, Polygon::Handle handle)
{
    if (p != m_hoverPolygon || index != m_hoverIndex || handle != m_hoverHandle)
    {
        m_hoverPolygon = p;
        m_hoverIndex = index;
        m_hoverHandle = handle;
        return true;
    }
    return false;
}

static inline float dot(const QPointF &p1, const QPointF &p2)
{
	return p1.x() * p2.x() + p1.y() * p2.y();
}

static float distance(const QPointF &pt, const QLineF &line, float &t)
{
	QPointF d = pt - line.p1();
	t = dot(line.p2(), d) / dot(line.p2(), line.p2());
	d = d - t * line.p2();
	return dot(d, d);
}

static QRectF boundingRect(const Polygon::Point &p1, const Polygon::Point &p2)
{
	float left = std::min<float>(p1.knot.x(), p1.knot.x() + p1.tan_out.x());
	left = std::min<float>(left, p2.knot.x());
	left = std::min<float>(left, p2.knot.x() + p2.tan_in.x());

	float right = std::max<float>(p1.knot.x(), p1.knot.x() + p1.tan_out.x());
	right = std::max<float>(right, p2.knot.x());
	right = std::max<float>(right, p2.knot.x() + p2.tan_in.x());

	float top = std::min<float>(p1.knot.y(), p1.knot.y() + p1.tan_out.y());
	top = std::min<float>(top, p2.knot.y());
	top = std::min<float>(top, p2.knot.y() + p2.tan_in.y());

	float bottom = std::max<float>(p1.knot.y(), p1.knot.y() + p1.tan_out.y());
	bottom = std::max<float>(bottom, p2.knot.y());
	bottom = std::max<float>(bottom, p2.knot.y() + p2.tan_in.y());

	return QRectF(left, top, right - left, bottom - top);
}

static bool pointOnSegment(const PolygonPointer &p, const QPointF &mouse, const Polygon::Point &p0, const Polygon::Point &p1, float t0, float t1, float threshold, float &t)
{
	// quick check to see if it's in the segment
	if (boundingRect(p0, p1).adjusted(-threshold, -threshold, threshold, threshold).contains(mouse))
	{
        static const int kSteps = 20;
        for (int i = 0; i < kSteps; i++)
        {
            float tt = float(i) / float(kSteps);
            float ti0 = t0 + tt * (t1 - t0);
            float ti1 = ti0 + 1.0f / float(kSteps);

            QPointF pt0 = p->eval(ti0);
            QPointF pt1 = p->eval(ti1);

            float t_out;
            if (Manipulator::pointOnLine(pt0, pt1, mouse, threshold, &t_out))
            {
                t = ti0 + (ti1 - ti0) * t_out;
                return true;
            }
        }
	}
	return false;
}

PolygonPointer PolygonManipulator::pointOnShape(const QPoint &pos, float dist, float *t_out) const
{
	auto shapes = Project::activeProject()->polygons(polygonType());
	for (auto shape : shapes)
	{
		float t = pointOnShape(shape, pos, dist);
		if (t >= 0.0f)
		{
			if (t_out)
				*t_out = t;
			return shape;
		}
	}

	return PolygonPointer();
}

float PolygonManipulator::pointOnShape(const PolygonPointer &p, const QPoint &pos, float dist) const
{
	float t;
	auto points = p->points();
	for (int i = 0; i < points.size(); i++)
	{
		const Polygon::Point &p0 = points[i];
		const Polygon::Point &p1 = i == points.size() - 1 ? points[0] : points[i + 1];
		if (pointOnSegment(p, pos, p0, p1, float(i), float(i + 1), dist / 2.0f, t))
			return t;
	}

	return -1.0f;
}

