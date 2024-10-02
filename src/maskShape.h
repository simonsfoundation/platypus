#ifndef MASKSHAPE_H
#define MASKSHAPE_H

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointF>
#include <QtGui/QPainterPath>
#include <QtCore/QVector>

class MaskShape;
typedef QSharedPointer<MaskShape> MaskShapePointer;

class MaskShape : public QObject
{
	Q_OBJECT
public:
	MaskShape(QObject *parent = nullptr);
	~MaskShape();

	enum Handle
	{
		kHandle_None,
		kHandle_Knot,
		kHandle_InTangent,
		kHandle_OutTangent
	};

	struct Point
	{
		QPointF knot;
		QPointF tan_in;
		QPointF tan_out;

		Point(const QPointF &knot, const QPointF &ti = QPointF(0.0f, 0.0f), const QPointF &to = QPointF(0.0f, 0.0f)) : knot(knot), tan_in(ti), tan_out(to) {}
		Point() {}

		bool isKnot() const { return tan_out == tan_in && tan_out.isNull(); }
	};

	typedef QVector<Point> PointList;

	void load(const class QDomElement &element);
	void save(class QDomElement &parent) const;

	void set(const QString &key, const QVariant &value);
	QVariant get(const QString &key) const;

	void set(const PointList &points);
	void set(int index, const Point &pt);
	const PointList &points() const;
	Point point(int index) const;

	// generate a polygon
	QPainterPath path() const;

	int insertPoint(float t);
	void deletePoint(int index);

Q_SIGNALS:
	void changed();
	void selectionChanged();

private:
	PointList m_points;
};

inline const MaskShape::PointList &MaskShape::points() const { return m_points; }
inline MaskShape::Point MaskShape::point(int index) const { return m_points[index]; }

#endif
