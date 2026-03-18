#include <project.h>
#include <polygon.h>
#include <imageManager.h>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtCore/QFile>
#include <QtGui/QPainter>

static Project *s_instance;

static const char *kRootName = "platypus";

Project *Project::activeProject()
{
	return s_instance;
}

Project::Project(QObject *parent) : QObject(parent)
{
	s_instance = this;
}

Project::~Project()
{
	s_instance = nullptr;
}

void Project::setPath(const QString &path)
{
	m_path = path;
}

void Project::setImagePath(const QString &path)
{
	m_imagePath = path;
}

void Project::clear()
{
	m_path = QString();
	m_imagePath = QString();
	m_polygons.clear();
    m_ms = CradleFunctions::MarkedSegments();

	emit changed();
}

void Project::load(const QByteArray &data)
{
    clear();
	QXmlStreamReader reader(data);

	// Find the root element
	while (!reader.atEnd() && !reader.hasError())
	{
		QXmlStreamReader::TokenType token = reader.readNext();
		if (token == QXmlStreamReader::StartElement && reader.name() == QLatin1String(kRootName))
		{
			// Read children of root
			while (reader.readNextStartElement())
			{
				if (reader.name() == QLatin1String("polygons"))
				{
					while (reader.readNextStartElement())
					{
						if (reader.name() == QLatin1String("polygon"))
						{
							PolygonPointer poly(new Polygon);
							poly->load(reader);
							addPolygon(poly);
						}
						else
						{
							reader.skipCurrentElement();
						}
					}
				}
				else if (reader.name() == QLatin1String("image"))
				{
					setImagePath(reader.readElementText());
				}
				else
				{
					reader.skipCurrentElement();
				}
			}
		}
	}

	if (reader.hasError())
	{
		throw reader.errorString();
	}
}

QByteArray Project::save() const
{
	QByteArray data;
	QXmlStreamWriter writer(&data);
	writer.setAutoFormatting(true);
	writer.writeStartDocument();
	writer.writeStartElement(kRootName);

	// image path
	writer.writeTextElement("image", m_imagePath);

	// polygons
	writer.writeStartElement("polygons");
	for (const auto &poly : m_polygons)
	{
		poly->save(writer);
	}
	writer.writeEndElement(); // polygons

	writer.writeEndElement(); // platypus
	writer.writeEndDocument();

	return data;
}

void Project::load(const QString &path)
{
	QFile file(path);
	if (file.open(QFile::ReadOnly | QFile::Text))
	{
		setPath(path);
		load(file.readAll());

		QString segPath = path + ".mask";
		if (QFile::exists(segPath))
			m_ms = CradleFunctions::readMarkedSegmentsFile(segPath.toStdString());
	}
}

void Project::save(const QString &path) const
{
	QFile file(path);
	if (file.open(QFile::WriteOnly | QFile::Text))
	{
		file.write(save());

		// save the marked segments
		QString segPath = path + ".mask";
		if (QFile::exists(segPath))
			QFile::remove(segPath);
		if (m_ms.pieces != 0)
			CradleFunctions::writeMarkedSegmentsFile(segPath.toStdString(), m_ms);
	}
	else
	{
		throw file.errorString();
	}
}

void Project::addPolygon(const PolygonPointer &p)
{
	m_polygons << p;
	connect(p.data(), &Polygon::changed, this, &Project::onPolygonChanged);
	connect(p.data(), &Polygon::valueChanged, this, &Project::onPolygonValueChanged);
	connect(p.data(), &Polygon::selectionChanged, this, &Project::onSelectionChanged);
	emit changed();
	emit polygonAdded(p.data());
}

void Project::removePolygon(const PolygonPointer &p)
{
	disconnect(p.data(), &Polygon::changed, this, &Project::onPolygonChanged);
	disconnect(p.data(), &Polygon::valueChanged, this, &Project::onPolygonValueChanged);
	disconnect(p.data(), &Polygon::selectionChanged, this, &Project::onSelectionChanged);
	m_polygons.removeAll(p);
	emit changed();
	emit polygonRemoved(p.data());
}

void Project::onPolygonChanged()
{
	emit polygonChanged(dynamic_cast<Polygon *>(sender()));
	emit changed();
}

void Project::onPolygonValueChanged(const QString &key)
{
	emit polygonValueChanged(dynamic_cast<Polygon *>(sender()), key);
}

void Project::onSelectionChanged()
{
	emit selectionChanged();
}

QList<PolygonPointer> Project::polygons(Polygon::Type type) const
{
	QList<PolygonPointer> result;
	for (auto p : m_polygons)
	{
		if (p->type() == type)
			result << p;
	}
	return result;
}

QList<PolygonPointer> Project::selection(Polygon::Type type) const
{
	QList<PolygonPointer> result;
	for (auto p : m_polygons)
	{
		if ((type == Polygon::NONE || p->type() == type) && p->isSelected())
			result << p;
	}
	return result;
}

bool Project::hasSelection(Polygon::Type type) const
{
	for (auto p : m_polygons)
	{
		if ((type == Polygon::NONE || p->type() == type) && p->isSelected())
			return true;
	}
	return false;
}

QPainterPath Project::maskPath(const QSize &size) const
{
	QPainterPath path;

	for (auto p : m_polygons)
    {
        if (p->type() == Polygon::MASK)
            path.addPath(p->path(size));
    }

	return path;
}

void Project::renderMask(QImage &image) const
{
	QPainter p(&image);

	p.setPen(Qt::NoPen);
	p.setBrush(Qt::white);

	p.drawPath(maskPath(image.size()));

	p.end();
}

void Project::setMarkedSegments(const CradleFunctions::MarkedSegments &ms)
{
    m_ms = ms;
}

