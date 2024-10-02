#ifndef PROJECT_H
#define PROJECT_H

#include <polygon.h>
#include <platypus/CradleFunctions.h>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

class Project :	public QObject
{
	Q_OBJECT
public:
	Project(QObject *parent = nullptr);
	~Project();

	static Project *activeProject();

	QString path() const;
	QString imagePath() const;

	void load(const QString &path);
	void save(const QString &path) const;

	void load(const QByteArray &data);
	QByteArray save() const;

	void setPath(const QString &path);
	void setImagePath(const QString &path);

	// polygons
	const PolygonList &polygons() const { return m_polygons; }
	PolygonList polygons(Polygon::Type type) const;
	void addPolygon(const PolygonPointer &p);
	void removePolygon(const PolygonPointer &p);

	PolygonList selection(Polygon::Type type = Polygon::NONE) const;
	bool hasSelection(Polygon::Type type = Polygon::NONE) const;

	// mask
	QPainterPath maskPath(const QSize &size) const;
	void renderMask(QImage &image) const;

	// marked segments
	void setMarkedSegments(const CradleFunctions::MarkedSegments &ms);
	const CradleFunctions::MarkedSegments &markedSegments() const { return m_ms; }

Q_SIGNALS:
	void changed();
	void selectionChanged();
	void polygonAdded(Polygon *p);
	void polygonRemoved(Polygon *p);
	void polygonChanged(Polygon *p);
  void polygonValueChanged(Polygon *p, const QString &key);

public slots:
	void clear();

private slots:
	void onPolygonChanged();
	void onPolygonValueChanged(const QString &);
	void onSelectionChanged();

private:
	QString m_path;
	QString m_imagePath;
	PolygonList m_polygons;
  CradleFunctions::MarkedSegments m_ms;
};

inline QString Project::path() const { return m_path; }
inline QString Project::imagePath() const { return m_imagePath; }

#endif
