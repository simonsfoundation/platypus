#include <maskShape.h>
#include <QtXml/QDomDocument>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtGui/QPolygonF>
#include <cmath>

MaskShape::MaskShape(QObject *parent) : QObject(parent)
{
}

MaskShape::~MaskShape()
{
}

void MaskShape::set(const QString &key, const QVariant &value)
{
}

QVariant MaskShape::get(const QString &key) const
{
	return QVariant();
}

void MaskShape::load(const QDomElement &root)
{
	QDomElement pointsNode = root.firstChildElement("points");
	QDomElement pointNode = pointsNode.firstChildElement("point");
	while (!pointNode.isNull())
	{
		QString text = pointNode.text();
		QStringList args = text.split(",");
		if (args.size() == 6)
		{
			QPointF knot(args[0].toFloat(), args[1].toFloat());
			QPointF ti(args[2].toFloat(), args[3].toFloat());
			QPointF to(args[4].toFloat(), args[5].toFloat());
			m_points << Point(knot, ti, to);
		}
		pointNode = pointNode.nextSiblingElement("point");
	}
}

void MaskShape::save(QDomElement &parent) const
{
	QDomElement root = parent.ownerDocument().createElement("shape");
	parent.appendChild(root);

	QDomElement points = parent.ownerDocument().createElement("points");
	root.appendChild(points);

	for (auto pt : m_points)
	{
		QDomElement point = parent.ownerDocument().createElement("point");
		points.appendChild(point);
		point.appendChild(parent.ownerDocument().createTextNode(
			QString("%1,%2,%3,%4,%5,%6").
				arg(pt.knot.x()).arg(pt.knot.y()).
				arg(pt.tan_in.x()).arg(pt.tan_in.y()).
				arg(pt.tan_out.x()).arg(pt.tan_out.y())
				));
	}
}

void MaskShape::set(const PointList &points)
{
	m_points = points;
	emit changed();
}

void MaskShape::set(int index, const Point &pt)
{
	m_points[index] = pt;
	emit changed();
}

inline float evalBez(float t, float p0, float p1, float p2, float p3)
{
	float mg0 =        -p0 + 3.0f * p1 - 3.0f * p2 + p3;
	float mg1 =  3.0f * p0 - 6.0f * p1 + 3.0f * p2;
	float mg2 = -3.0f * p0 + 3.0f * p1;
	float mg3 =         p0;
	return t * t * t * mg0 + t * t * mg1 + t * mg2 + mg3;
}

QPainterPath MaskShape::path() const
{
	QPainterPath path;
	path.moveTo(m_points[0].knot);
	for (int i = 1; i < m_points.size(); i++)
		path.cubicTo(m_points[i - 1].tan_out + m_points[i - 1].knot, m_points[i].tan_in + m_points[i].knot, m_points[i].knot);
	if (m_points.size() > 1)
		path.cubicTo(m_points.back().tan_out + m_points.back().knot, m_points.front().tan_in + m_points.front().knot, m_points.front().knot);
	path.closeSubpath();
	return path;
}

static inline QPointF subdivide(const QPointF  &p0, const QPointF  &p1, float t)
{
	return (p0 * (1.0f - t)) + (p1 * t);
}

int MaskShape::insertPoint(float t)
{
	int segment = int(std::floor(t));
	float s = t - float(segment);

	Point &prev = m_points[segment];
	Point &next = m_points[segment == m_points.size() - 1 ? 0 : segment + 1];
	bool prevKnot = prev.isKnot();
	bool nextKnot = next.isKnot();

	// calculate tangents for new and surrounding points
	// to retain curve shape
	const QPointF &c1 = prev.knot;
	QPointF c2 = prev.tan_out + prev.knot;
	QPointF c3 = next.tan_in + next.knot;
	const QPointF &c4 = next.knot;

	QPointF b10 = subdivide(c1, c2, s);
	QPointF b11 = subdivide(c2, c3, s);
	QPointF b12 = subdivide(c3, c4, s);
	QPointF b20 = subdivide(b10, b11, s);
	QPointF b21 = subdivide(b11, b12, s);
	QPointF b30 = subdivide(b20, b21, s);

	prev.tan_out = b10 - prev.knot;
	next.tan_in = b12 - next.knot;

	Point pt;
	pt.knot = b30;
	if (true) // !prevKnot || !nextKnot)
	{
		pt.tan_in = b20 - pt.knot;
		pt.tan_out = b21 - pt.knot;
	}
	else
	{
		pt.tan_in = pt.tan_out = QPointF(0.0f, 0.0f);
	}

	int index = segment + 1;
	m_points.insert(m_points.begin() + index, pt);

	emit changed();

	return index;
}

void MaskShape::deletePoint(int index)
{
	m_points.removeAt(index);
}
