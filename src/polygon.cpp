#include <polygon.h>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtCore/QMimeData>
#include <QtCore/QLine>
#include <QtXml/QDomDocument>
#include <cmath>

Polygon::Polygon(QObject *parent) : QObject(parent), m_selected(false), m_type(NONE)
{
    setDefaults();
}

Polygon::Polygon(Type type, QObject *parent) : QObject(parent), m_selected(false), m_type(type)
{
    setDefaults();
}

Polygon::Polygon(const Polygon &rhs) : m_selected(false), m_points(rhs.m_points), m_values(rhs.m_values), m_type(rhs.m_type), m_color(rhs.m_color)
{
}

Polygon::~Polygon()
{
}

void Polygon::setDefaults()
{
    setValue("black", 0);
    setValue("gamma", 0);
    setValue("white", 255);
}

Polygon *Polygon::clone() const
{
	return new Polygon(*this);
}

void Polygon::load(const QDomElement &root)
{
	QString type = root.attribute("type", "input");
	if (type == "input")
		m_type = INPUT;
	else if (type == "output")
		m_type = OUTPUT;
	else if (type == "mask")
		m_type = MASK;

	Q_ASSERT(this->type() != NONE);

	QDomElement valuesNode = root.firstChildElement("values");
    if (!valuesNode.isNull())
    {
        QDomElement child = valuesNode.firstChildElement();
        while (!child.isNull())
        {
            QString tagName = child.tagName();
            QString text = child.text();
            m_values[tagName] = text;
            child = child.nextSiblingElement();
        }
    }

	QDomElement pointsNode = root.firstChildElement("points");
	QDomElement pointNode = pointsNode.firstChildElement("point");
	while (!pointNode.isNull())
	{
		QString text = pointNode.text();
		QStringList args = text.split(",");
		if (args.size() == 2)
		{
			QPoint pt(args[0].toInt(), args[1].toInt());
			m_points << pt;
		}
		else if (args.size() == 6)
		{
			QPoint knot(args[0].toInt(), args[1].toInt());
			QPoint tan_in(args[2].toInt(), args[3].toInt());
			QPoint tan_out(args[4].toInt(), args[5].toInt());
			m_points << Point(knot, tan_in, tan_out);
		}
		pointNode = pointNode.nextSiblingElement("point");
	}
}

void Polygon::save(QDomElement &parent) const
{
	Q_ASSERT(type() != NONE);

	QDomElement root = parent.ownerDocument().createElement("polygon");
	parent.appendChild(root);

	root.setAttribute("type", m_type == INPUT ? "input" : m_type == OUTPUT ? "output" : "mask");

    // write properties
	QDomElement values = parent.ownerDocument().createElement("values");
	root.appendChild(values);
    for (auto key : m_values.keys())
    {
        QVariant v = m_values.value(key);
		QDomElement value = parent.ownerDocument().createElement(key);
		values.appendChild(value);
        value.appendChild(parent.ownerDocument().createTextNode(v.toString()));
    }

    // write points
	QDomElement points = parent.ownerDocument().createElement("points");
	root.appendChild(points);

	for (auto pt : m_points)
	{
		QDomElement point = parent.ownerDocument().createElement("point");
		points.appendChild(point);
        if (pt.tan_in.isNull() && pt.tan_out.isNull())
            point.appendChild(parent.ownerDocument().createTextNode(QString("%1,%2").arg(pt.knot.x()).arg(pt.knot.y())));
        else
            point.appendChild(parent.ownerDocument().createTextNode(QString("%1,%2,%3,%4,%5,%6").
                arg(pt.knot.x()).arg(pt.knot.y()).
                arg(pt.tan_in.x()).arg(pt.tan_in.y()).
                arg(pt.tan_out.x()).arg(pt.tan_out.y())
                ));
	}
}

void Polygon::set(const QPolygon &poly)
{
	Q_ASSERT(type() != NONE);

    m_points.clear();
    for (auto p : poly)
        m_points << p;
	emit changed();
}

void Polygon::set(const QRect &rect)
{
    m_points.clear();
    m_points << rect.topLeft();
    m_points << rect.topRight();
    m_points << rect.bottomRight();
    m_points << rect.bottomLeft();
	emit changed();
}

void Polygon::setSelected(bool state)
{
	Q_ASSERT(type() != NONE);

	if (state != m_selected)
	{
		m_selected = state;
		emit selectionChanged();
	}
}

QVariant Polygon::value(const QString &key) const
{
	return m_values[key];
}

void Polygon::setValue(const QString &key, const QVariant &value)
{
	if (key == "color")
		m_color = qvariant_cast<QColor>(value);
    else
    {
        if (m_values[key] != value)
        {
            m_values[key] = value;
            emit changed();
            emit valueChanged(key);
        }
    }
}

QRect Polygon::boundingRect() const
{
    int left = std::numeric_limits<int>::max();
    int top = std::numeric_limits<int>::max();
    int right = -std::numeric_limits<int>::max();
    int bottom = -std::numeric_limits<int>::max();

    for (auto p : m_points)
    {
        left = std::min(left, p.knot.x());
        top = std::min(top, p.knot.y());
        right = std::max(right, p.knot.x());
        bottom = std::max(bottom, p.knot.y());
    }
    return QRect(left, top, right - left, bottom - top);
}

bool Polygon::isHorizontal() const
{
    QRect rect = boundingRect();
    return rect.width() > rect.height();
}

QLine Polygon::centerLine() const
{
    Q_ASSERT(type() == INPUT);
    Q_ASSERT(shapeType() == POLYGON);
	PointList points = this->points();
	if (isHorizontal())
	{
		qSort(points.begin(), points.end(), [&](const Point &lhs, const Point &rhs) {
			if (lhs.knot.x() == rhs.knot.x())
				return lhs.knot.y() < rhs.knot.y();
			return lhs.knot.x() < rhs.knot.x();
		});
	}
	else
	{
		qSort(points.begin(), points.end(), [&](const Point &lhs, const Point &rhs) {
			if (lhs.knot.y() == rhs.knot.y())
				return lhs.knot.x() < rhs.knot.x();
			return lhs.knot.y() < rhs.knot.y();
		});

	}
	QPoint p0 = (points[0].knot + points[1].knot) / 2;
	QPoint p1 = (points[2].knot + points[3].knot) / 2;
	return QLine(p0, p1);
}

Polygon::Point Polygon::point(int index) const
{
    return m_points[index];
}

void Polygon::setPoint(int index, const Point &point)
{
    m_points[index] = point;
    emit changed();
}

void Polygon::set(const PointList &points)
{
    m_points = points;
    emit changed();
}

QPainterPath Polygon::path(const QSize &size) const
{
    QPainterPath path;
    if (shapeType() == BEZIER)
    {
        path.moveTo(m_points[0].knot);
        for (int i = 1; i < m_points.size(); i++)
            path.cubicTo(m_points[i - 1].tan_out + m_points[i - 1].knot, m_points[i].tan_in + m_points[i].knot, m_points[i].knot);
        if (m_points.size() > 1)
            path.cubicTo(m_points.back().tan_out + m_points.back().knot, m_points.front().tan_in + m_points.front().knot, m_points.front().knot);
    }
    else
    {
        path.moveTo(m_points[0].knot);
        for (int i = 1; i < m_points.size(); i++)
            path.lineTo(m_points[i].knot);
    }
    path.closeSubpath();

    // invert?
    if (size.isValid() && value("invert").toBool())
    {
        QRect rect(0, 0, size.width(), size.height());
        path.addRect(rect);
    }

    return path;
}

QColor Polygon::color() const
{
    if (m_color.isValid())
        return m_color;
    return m_type == MASK ? Qt::red : Qt::blue;
}

Polygon::ShapeType Polygon::shapeType() const
{
    return type() == MASK ? BEZIER : POLYGON;
}

void Polygon::translate(const QPoint &pt)
{
    for (auto &p : m_points)
        p.knot += pt;
}

static inline QPoint subdivide(const QPoint &p0, const QPoint &p1, float t)
{
	return ((QPointF(p0) * (1.0f - t)) + (QPointF(p1) * t)).toPoint();
}

int Polygon::insertPoint(float t)
{
	int segment = int(std::floor(t));
	float s = t - float(segment);

	Point &prev = m_points[segment];
	Point &next = m_points[segment == m_points.size() - 1 ? 0 : segment + 1];
	bool prevKnot = prev.isKnot();
	bool nextKnot = next.isKnot();

	// calculate tangents for new and surrounding points
	// to retain curve shape
	const QPoint &c1 = prev.knot;
	QPoint c2 = prev.tan_out + prev.knot;
	QPoint c3 = next.tan_in + next.knot;
	const QPoint &c4 = next.knot;

	QPoint b10 = subdivide(c1, c2, s);
	QPoint b11 = subdivide(c2, c3, s);
	QPoint b12 = subdivide(c3, c4, s);
	QPoint b20 = subdivide(b10, b11, s);
	QPoint b21 = subdivide(b11, b12, s);
	QPoint b30 = subdivide(b20, b21, s);

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
		pt.tan_in = pt.tan_out = QPoint(0, 0);
	}

	int index = segment + 1;
	m_points.insert(m_points.begin() + index, pt);

	emit changed();

	return index;
}

void Polygon::deletePoint(int index)
{
	m_points.removeAt(index);
}

static inline float evalBez(float t, float p0, float p1, float p2, float p3)
{
	float mg0 =        -p0 + 3.0f * p1 - 3.0f * p2 + p3;
	float mg1 =  3.0f * p0 - 6.0f * p1 + 3.0f * p2;
	float mg2 = -3.0f * p0 + 3.0f * p1;
	float mg3 =         p0;
	return t * t * t * mg0 + t * t * mg1 + t * mg2 + mg3;
}

static QPointF evalBez(float t, const QPoint &p0, const QPoint &p1, const QPoint &p2, const QPoint &p3)
{
    return QPointF(evalBez(t, p0.x(), p1.x(), p2.x(), p3.x()), evalBez(t, p0.y(), p1.y(), p2.y(), p3.y()));
}

QPointF Polygon::eval(float t) const
{
    int seg = int(std::floor(t));
    t = t - float(seg);

    if (seg == m_points.size())
        seg = 0;

    const Point &p0 = m_points[seg];
    const Point &p1 = seg == m_points.size() - 1 ? m_points[0] : m_points[seg + 1];
    return evalBez(t, p0.knot, p0.knot + p0.tan_out, p1.knot + p1.tan_in, p1.knot);
}
