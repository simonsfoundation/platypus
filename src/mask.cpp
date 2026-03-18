#include <mask.h>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
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

void Mask::load(QXmlStreamReader &reader)
{
	// reader is positioned at the <mask> or parent element containing <shapes>
	while (reader.readNextStartElement())
	{
		if (reader.name() == QLatin1String("shapes"))
		{
			while (reader.readNextStartElement())
			{
				if (reader.name() == QLatin1String("shape"))
				{
					MaskShapePointer shape(new MaskShape);
					shape->load(reader);
					addShape(shape);
				}
				else
				{
					reader.skipCurrentElement();
				}
			}
		}
		else
		{
			reader.skipCurrentElement();
		}
	}
}

void Mask::save(QXmlStreamWriter &writer) const
{
	writer.writeStartElement("shapes");
	for (const auto &shape : m_shapes)
		shape->save(writer);
	writer.writeEndElement(); // shapes
}

void Mask::addShape(const MaskShapePointer &mask)
{
	m_shapes << mask;
	connect(mask.data(), &MaskShape::changed, this, &Mask::changed);
	emit changed();
}

void Mask::removeShape(const MaskShapePointer &mask)
{
	disconnect(mask.data(), &MaskShape::changed, this, &Mask::changed);
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

