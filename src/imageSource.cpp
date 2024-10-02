#include <imageSource.h>

ImageSource::ImageSource(QObject *parent) : QObject(parent)
{
}

void ImageSource::invalidate()
{
	emit changed();
}
