#include <project.h>
#include <polygon.h>
#include <imageManager.h>
#include <QtXml/QDomDocument>
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
	QDomDocument doc;
	QString errorStr;
	int errorLine;
	int errorColumn;
	if (doc.setContent(data, true, &errorStr, &errorLine, &errorColumn))
	{
		QDomElement root = doc.documentElement();
		QString tag = root.tagName();
		if (root.tagName() == kRootName)
		{
			// polygons
			QDomElement polygonsNode = root.firstChildElement("polygons");
			QDomElement polygonNode = polygonsNode.firstChildElement("polygon");
			while (!polygonNode.isNull())
			{
				PolygonPointer poly(new Polygon);
				poly->load(polygonNode);
				addPolygon(poly);

				polygonNode = polygonNode.nextSiblingElement("polygon");
			}

			#if 0

            // segments
			QDomElement segmentsNode = root.firstChildElement("segments");
            if (segmentsNode.isElement())
            {
                // pieces
                QDomElement pieceNode = segmentsNode.firstChildElement("piece");
                while (!pieceNode.isNull())
                {
                    QString text = pieceNode.text();
                    QStringList args = text.split(' ');
                    if (args.size() == 3)
                    {
                        int type = args[0].toInt();
                        int x = args[1].toInt();
                        int y = args[2].toInt();

                        m_ms.piece_type.push_back(type);
                        m_ms.piece_middle.push_back(cv::Point2i(x, y));
                    }

                    pieceNode = pieceNode.nextSiblingElement();
                }

                // mask
                QDomElement maskNode = segmentsNode.firstChildElement("mask");
                if (!maskNode.isNull())
                {
                    const cvImageRef &mask = ImageManager::get().removeMask();

                    m_ms.piece_mask = cv::Mat(mask->height, mask->width, CV_16U, cv::Scalar(0));

                    QString text = maskNode.text();
                    QByteArray data = QByteArray::fromBase64(text.toLatin1());
                    data = qUncompress(data);
                    if (mask->imageSize == data.size())
                        std::memcpy(m_ms.piece_mask.data, data.constData(), data.size());
                }
            }
			#endif
		}
	}
	else
	{
		throw errorStr;
	}
}

QByteArray Project::save() const
{
	QDomDocument doc("platpypus");
	QDomElement root = doc.createElement(kRootName);
	doc.appendChild(root);

	// image path
	{
		QDomElement elem = doc.createElement("image");
		root.appendChild(elem);
		elem.appendChild(doc.createTextNode(m_imagePath));
	}

	// polygons
    int outputCount = 0;
	{
		QDomElement polygons = doc.createElement("polygons");
		root.appendChild(polygons);
		for (auto poly : m_polygons)
        {
			poly->save(polygons);
            if (poly->type() == Polygon::OUTPUT)
                outputCount++;
        }
	}

    // marked segments
    if (m_ms.pieces != 0 && 0)
    {
		QDomElement segments = doc.createElement("segments");
		root.appendChild(segments);

        for (int i = 0; i < m_ms.pieces; i++)
        {
            QDomElement piece = doc.createElement("piece");
            segments.appendChild(piece);
            int type = m_ms.piece_type[i];
            const cv::Point2i &pt = m_ms.piece_middle[i];
            piece.appendChild(doc.createTextNode(QString("%1,%2,%3").arg(type).arg(pt.x).arg(pt.y)));
        }

        const cv::Mat& mask = m_ms.piece_mask;
        QByteArray sourceData(reinterpret_cast<const char*>(mask.data), mask.size[0] * mask.size[1] * sizeof(short));
        QByteArray compressed = qCompress(sourceData);
        compressed = compressed.toBase64();
        QDomElement maskElement = doc.createElement("mask");
        segments.appendChild(maskElement);
        maskElement.appendChild(doc.createTextNode(compressed));
    }

    // mask
    #if 0
    if (outputCount != 0)
    {
        cvImageRef mask = ImageManager::get().removeMask();
        QByteArray sourceData(mask->imageData, mask->imageSize);
        QByteArray compressed = qCompress(sourceData);
        compressed = compressed.toBase64();
        QDomElement maskElement = doc.createElement("mask");
        root.appendChild(maskElement);
        maskElement.appendChild(doc.createTextNode(compressed));
    }
    #endif

	return doc.toByteArray();
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
	connect(p.data(), SIGNAL(changed()), this, SLOT(onPolygonChanged()));
	connect(p.data(), SIGNAL(valueChanged(const QString &)), this, SLOT(onPolygonValueChanged(const QString &)));
	connect(p.data(), SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
	emit changed();
	emit polygonAdded(p.data());
}

void Project::removePolygon(const PolygonPointer &p)
{
	p->disconnect(SIGNAL(changed()), this, SLOT(onPolygonChanged()));
	p->disconnect(SIGNAL(valueChanged(const QString &)), this, SLOT(onPolygonValueChanged(const QString &)));
	p->disconnect(SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
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

