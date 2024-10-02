#ifndef POLYGON_H
#define POLYGON_H

#include <QtCore/QObject>
#include <QtGui/QColor>
#include <QtGui/QPolygon>
#include <QtGui/QPainterPath>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>

class Polygon;
typedef QSharedPointer<Polygon> PolygonPointer;
typedef QList<PolygonPointer> PolygonList;

class Polygon :	public QObject
{
	Q_OBJECT
public:	
	enum Type
	{
		NONE,
		INPUT,
		OUTPUT,
		MASK
	};
    
    enum ShapeType
    {
        POLYGON,
        BEZIER
    };

	Polygon(QObject *parent = nullptr);
	Polygon(Type type, QObject *parent = nullptr);
	Polygon(const Polygon &rhs);
	~Polygon();
    
	enum Handle
	{
		kHandle_None,
		kHandle_Knot,
		kHandle_InTangent,
		kHandle_OutTangent
	};

	struct Point
	{
		QPoint knot;
		QPoint tan_in;
		QPoint tan_out;

		Point(const QPoint &knot, const QPoint &ti = QPoint(0, 0), const QPoint &to = QPoint(0, 0)) : knot(knot), tan_in(ti), tan_out(to) {}
		Point() {}
        
		bool isKnot() const { return tan_out == tan_in && tan_out.isNull(); }
	};

	typedef QVector<Point> PointList;

	Type type() const;
    ShapeType shapeType() const;
    QColor color() const;
    
    bool isHorizontal() const;
	QLine centerLine() const;
    QRect boundingRect() const;

	virtual Polygon *clone() const;

	void load(const class QDomElement &element);
	void save(class QDomElement &parent) const;

    QPainterPath path(const QSize &size = QSize()) const;
	bool isSelected() const;

	QVariant value(const QString &key) const;
	void setValue(const QString &key, const QVariant &value);
    
	const PointList &points() const;
    Point point(int index) const;
    void setPoint(int index, const Point &point);
    void set(const PointList &points);
    
    void translate(const QPoint &pt);

	int insertPoint(float t);
	void deletePoint(int index);
    
    QPointF eval(float t) const;

Q_SIGNALS:
	void changed();
    void valueChanged(const QString &key);
	void selectionChanged();

public slots:
	void set(const QPolygon &poly);
	void set(const QRect &rect);
	void setSelected(bool state);

protected:
    void setDefaults();

private:
	PointList m_points;
	QVariantMap m_values;
	Type m_type;
	bool m_selected;
    QColor m_color;
};

inline Polygon::Type Polygon::type() const { return m_type; }
inline bool Polygon::isSelected() const { return m_selected; }
inline const Polygon::PointList &Polygon::points() const { return m_points; }

#endif
