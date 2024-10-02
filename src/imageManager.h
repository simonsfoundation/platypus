#ifndef IMAGEMANAGER_H
#define IMAGEMANAGER_H

#include <cvImage.h>
#include <QtCore/QObject>
#include <QtGui/QImage>
#include <QtCore/QFutureWatcher>

class ImageSource;
class RemoveSource;

class ImageManager : public QObject
{
Q_OBJECT
public:
	ImageManager(QObject *parent = nullptr);
	~ImageManager();

	static ImageManager &get();

	void load(const QString &path);
	void clear();

	ImageSource *source() const { return m_source; }
	RemoveSource *removeSource() const { return m_removeSource; }

    QSize size() const;

    const cvImageRef &floatImage() const { return m_float; }
    const cvImageRef &resultImage() const { return m_result; }
    const cvImageRef &removeMask() const { return m_removeMask; }

Q_SIGNALS:
	void imageChanged();
	void status(const QString &msg);

private:
	ImageSource *m_source;
	RemoveSource *m_removeSource;
	QFutureWatcher<ImageSource *> *m_watcher;
    cvImageRef m_float;
    cvImageRef m_result;
    cvImageRef m_removeMask;

private:
	ImageSource *loadFunc(const QString &path);

private slots:
	void onLoadFinished();
};

#endif
