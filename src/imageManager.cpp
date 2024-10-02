#include <imageManager.h>
#include <imageSource.h>
#include <removeSource.h>
#include <opencv2/core/core_c.h>
#include <QtWidgets/QMessageBox>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>

static ImageManager *s_instance;

static const int kMaxImageSize = 32768;
static const int kMinSize = 1024;

static QImage scale(const QImage &source, const QSize &size)
{
    QImage result(size, QImage::Format_Indexed8);

    int width = size.width();
    int height = size.height();

    result.setColorCount(256);
    for (int i = 0; i < 256; i++)
        result.setColor(i, qRgba(i, i, i, 255));

    for (int y = 0; y < height; y++)
    {
        int sy = y * 2;
        const uchar *srow0 = source.scanLine(sy);
        const uchar *srow1 = sy < source.height() - 1 ? source.scanLine(sy + 1) : srow0;
        uchar *drow = result.scanLine(y);
        const uchar *end = drow + width;
        while (drow != end)
        {
            *drow = (srow0[0] + srow0[1] + srow1[0] + srow1[1]) / 4;
            srow0 += 2;
            srow1 += 2;
            drow++;
        }
    }

    return result;
}

class MipMappedSource : public ImageSource
{
	QList<QImage> m_levels;
public:
	MipMappedSource(const QString &path)
	{
		QImage image(path);
        if (!image.isNull())
        {
            // convert to gray if necessary
            QImage::Format format = image.format();
            if (format != QImage::Format_Indexed8)
            {
                QVector<QRgb> colors(256);
                for (int i = 0; i < colors.size(); i++)
                    colors[i] = qRgba(i, i, i, 255);
                image = image.convertToFormat(QImage::Format_Indexed8, colors, Qt::ThresholdDither);
            }

            m_levels.push_back(image);
            QSize size = image.size();
            while (size.width() > kMinSize || size.height() > kMinSize)
            {
                size = size / 2;
                image = scale(image, size);
                m_levels.push_back(image);
            }
        }
	}

	bool tooBig() const
	{
		QSize size = this->size();
		return size.width() > kMaxImageSize || size.height() > kMaxImageSize;
	}

	virtual QSize size() const
	{
		if (m_levels.empty())
			return QSize(0, 0);
		return m_levels[0].size();
	}
	virtual QString name() const { return "Image"; }

	virtual bool isValid() const
	{
		if (m_levels.empty() || tooBig())
			return false;
		return true;
	}

	virtual QImage getTile(const QRect &tile, const QSize &size) const
	{
		// find the best image to use - ie. the smallest image that contains the tile
		float scale = float(size.width()) / float(tile.width());
        int level = int(std::sqrt(1.0f / scale)) + 1;
        if (scale > 0.5f)
            level = 1;
		level = std::max(level, 1);
		level = std::min(level, m_levels.size());

		int ds = int(std::pow(2, level - 1));

		QRect readTile(tile.left() / ds, tile.top() / ds, tile.width() / ds, tile.height() / ds);
		QImage result(size, QImage::Format_Indexed8);
		result.setColorCount(256);
		for (int i = 0; i < 256; i++)
			result.setColor(i, qRgba(i, i, i, 255));

		const QImage &source = m_levels[level - 1];

		cv::Mat sourceWrap(readTile.height(), readTile.width(), CV_8U, (void *)(source.bits() + readTile.top() * source.bytesPerLine() + readTile.left()), source.bytesPerLine());
		cv::Mat resultWrap(result.height(), result.width(), CV_8U, result.bits(), result.bytesPerLine());

		cv::resize(sourceWrap, resultWrap, resultWrap.size());

		return result;
	}
};

ImageManager &ImageManager::get()
{
	Q_ASSERT(s_instance);
	return *s_instance;
}

ImageManager::ImageManager(QObject *parent) : QObject(parent), m_source(nullptr)
{
	Q_ASSERT(s_instance == nullptr);
	s_instance = this;

	m_removeSource = new RemoveSource(this);

	m_watcher = new QFutureWatcher<ImageSource *>(this);
	connect(m_watcher, SIGNAL(finished()), SLOT(onLoadFinished()));
}

ImageManager::~ImageManager()
{
    clear();

	Q_ASSERT(s_instance == this);
	s_instance = nullptr;
}

QSize ImageManager::size() const
{
    return m_source ? m_source->size() : QSize();
}

void ImageManager::load(const QString &path)
{
	clear();

	emit status("Loading image...");

	QFuture<ImageSource *>future = QtConcurrent::run(this, &ImageManager::loadFunc, path);
	m_watcher->setFuture(future);
}

void ImageManager::clear()
{
    m_float = nullptr;
    m_result = nullptr;
    m_removeMask = nullptr;

	delete m_source;
	m_source = nullptr;

	emit imageChanged();
}

ImageSource *ImageManager::loadFunc(const QString &path)
{
	ImageSource *source = new MipMappedSource(path);

    return source;
}

void ImageManager::onLoadFinished()
{
	Q_ASSERT(m_source == nullptr);

    MipMappedSource *source = dynamic_cast<MipMappedSource *>(m_watcher->result());
	assert(source);
	if (source && source->isValid())
    {
        m_source = source;

        CvSize size = cvSize(source->size().width(), source->size().height());
        m_float = cvCreateImage(size, IPL_DEPTH_32F, 1);
        m_result = cvCreateImage(size, IPL_DEPTH_32F, 1);
        m_removeMask = cvCreateImage(size, IPL_DEPTH_16U, 1);

        // convert to float
        QImage image = source->getTile(QRect(QPoint(0, 0), source->size()), source->size());
        IplImage *header = cvCreateImageHeader(size, IPL_DEPTH_8U, 1);
        cvSetData(header, image.bits(), image.bytesPerLine());
        cvConvertScale(header, m_float, 1.0f, 0.0f);
        cvReleaseImageHeader(&header);

        // copy the input to the result
        cv::Mat(cv::cvarrToMat(m_float.get())).copyTo(cv::cvarrToMat(m_result.get()));
    }
    else
	{
		if (source && source->tooBig())
		{
			QMessageBox::critical(nullptr, QCoreApplication::applicationName(), tr("The image is too large. It must be smaller than 32728x32768 pixels."));
		}
        delete source;
	}

	emit status(QString());

	emit imageChanged();
}
