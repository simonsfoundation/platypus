#ifndef MASK_H
#define MASK_H

#include <maskShape.h>
#include <QtCore/QObject>
#include <QtGui/QPainterPath>

class QImage;

class Mask : public QObject
{
	Q_OBJECT
public:
	Mask(QObject *parent = nullptr);
	~Mask();

  typedef QList<MaskShapePointer> ShapeList;
  ShapeList m_shapes;
	void load(const class QDomElement &element);
	void save(class QDomElement &parent) const;

	void addShape(const MaskShapePointer &mask);
	void removeShape(const MaskShapePointer &mask);

	const ShapeList &shapes() const;

	void render(QImage &image) const;
	QPainterPath path() const;

Q_SIGNALS:
	void changed();

public slots:
	void clear();
};

#endif
