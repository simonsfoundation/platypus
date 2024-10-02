#include <mask.h>
#include <QtXml/QDomDocument>
#include <QtGui/QImage>
#include <QtGui/QPainter>

Mask::Mask(QObject *parent) : QObject(parent)
{
}

Mask::~Mask()
{
}

void Mask::clear()
{
	m_shapes.clear();
	emit changed();
}

void Mask::load(const QDomElement &root)
{
	QDomElement shapesNode = root.firstChildElement("shapes");
	QDomElement shapeNode = shapesNode.firstChildElement("shape");
	while (!shapeNode.isNull())
	{
		MaskShapePointer shape(new MaskShape);
		shape->load(shapeNode);
		addShape(shape);

		shapeNode  = shapeNode.nextSiblingElement("shape");
	}
}

void Mask::save(QDomElement &parent) const
{
	QDomElement shapes = parent.ownerDocument().createElement("shapes");
	parent.appendChild(shapes);
	for (auto shape : m_shapes)
		shape->save(shapes);
}

void Mask::addShape(const MaskShapePointer &mask)
{
	m_shapes << mask;
	connect(mask.data(), SIGNAL(changed()), SIGNAL(changed()));
	emit changed();
}

void Mask::removeShape(const MaskShapePointer &mask)
{
	mask->disconnect(SIGNAL(changed()), this, SIGNAL(changed()));
	m_shapes.removeAll(mask);
	emit changed();
}

QPainterPath Mask::path() const
{
	QPainterPath path;

	for (auto shape : m_shapes)
		path.addPath(shape->path());

	return path;
}

void Mask::render(QImage &image) const
{
	QPainter p(&image);

	p.setPen(Qt::NoPen);
	p.setBrush(Qt::white);

	p.drawPath(path());

	p.end();
}

