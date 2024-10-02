#ifndef IMAGE_SOURCE_H
#define IMAGE_SOURCE_H

#include <QtCore/QObject>
#include <QtGui/QImage>

class ImageSource : public QObject
{
	Q_OBJECT

public:
	ImageSource(QObject *parent = NULL);

	virtual QSize size() const = 0;
	virtual QString name() const = 0;

	virtual bool isValid() const = 0;

	virtual QImage getTile(const QRect &tile, const QSize &size) const = 0;

public slots:
 	virtual void invalidate();

Q_SIGNALS:
	void changed();
};

#endif

