#include <imageManager.h>
#include <imageSource.h>
#include <removeSource.h>
#include <opencv2/core/core_c.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <QtGui/QImageReader>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QFileInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>
#include <limits>

static ImageManager *s_instance;

static const int kMaxImageSize = 32768;
static const int kMinSize = 1024;

namespace
{
QVector<QRgb> grayColorTable()
{
    static const QVector<QRgb> colors = [] {
        QVector<QRgb> table(256);
        for (int i = 0; i < table.size(); ++i)
            table[i] = qRgba(i, i, i, 255);
        return table;
    }();
    return colors;
}

QImage makeIndexed8Image(const QSize &size)
{
    QImage image(size, QImage::Format_Indexed8);
    image.setColorTable(grayColorTable());
    return image;
}

QImage copyGray8ToIndexed8(const QImage &image)
{
    if (image.isNull())
        return QImage();

    QImage result = makeIndexed8Image(image.size());
    for (int y = 0; y < image.height(); ++y)
        memcpy(result.scanLine(y), image.constScanLine(y), size_t(image.width()));
    return result;
}

QImage normalizeGray16ToIndexed8(const QImage &image)
{
    if (image.isNull() || image.format() != QImage::Format_Grayscale16)
        return QImage();

    quint16 minValue = std::numeric_limits<quint16>::max();
    quint16 maxValue = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        const quint16 *row = reinterpret_cast<const quint16 *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            minValue = std::min(minValue, row[x]);
            maxValue = std::max(maxValue, row[x]);
        }
    }

    QImage result = makeIndexed8Image(image.size());
    if (maxValue == minValue)
    {
        const uchar fill = uchar((double(maxValue) / 65535.0) * 255.0 + 0.5);
        result.fill(fill);
        return result;
    }

    const double scale = 255.0 / double(maxValue - minValue);
    for (int y = 0; y < image.height(); ++y)
    {
        const quint16 *srow = reinterpret_cast<const quint16 *>(image.constScanLine(y));
        uchar *drow = result.scanLine(y);
        for (int x = 0; x < image.width(); ++x)
            drow[x] = uchar((double(srow[x] - minValue) * scale) + 0.5);
    }

    return result;
}

QImage decodeQtImageToWorking(const QImage &image)
{
    if (image.isNull())
        return QImage();

    if (image.format() == QImage::Format_Indexed8)
    {
        if (image.allGray())
        {
            QImage result = image.copy();
            result.setColorTable(grayColorTable());
            return result;
        }
        return copyGray8ToIndexed8(image.convertToFormat(QImage::Format_Grayscale8));
    }

    if (image.format() == QImage::Format_Grayscale8)
        return copyGray8ToIndexed8(image);

    if (image.format() == QImage::Format_Grayscale16)
        return normalizeGray16ToIndexed8(image);

    return copyGray8ToIndexed8(image.convertToFormat(QImage::Format_Grayscale8));
}

QImage normalizeCvGrayToIndexed8(const cv::Mat &gray)
{
    if (gray.empty() || gray.channels() != 1)
        return QImage();

    cv::Mat normalized;
    if (gray.depth() == CV_8U)
    {
        normalized = gray;
    }
    else
    {
        double minValue = 0.0;
        double maxValue = 0.0;
        cv::minMaxLoc(gray, &minValue, &maxValue);
        if (maxValue <= minValue)
        {
            gray.convertTo(normalized, CV_8U, 255.0 / 65535.0);
        }
        else
        {
            cv::normalize(gray, normalized, 0.0, 255.0, cv::NORM_MINMAX, CV_8U);
        }
    }

    QImage result = makeIndexed8Image(QSize(normalized.cols, normalized.rows));
    for (int y = 0; y < normalized.rows; ++y)
        memcpy(result.scanLine(y), normalized.ptr(y), size_t(normalized.cols));
    return result;
}

QImage decodeOpenCvImageToWorking(const cv::Mat &image)
{
    if (image.empty())
        return QImage();

    cv::Mat gray;
    if (image.channels() == 1)
    {
        gray = image;
    }
    else if (image.channels() == 3)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    }
    else if (image.channels() == 4)
    {
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    }
    else
    {
        return QImage();
    }

    return normalizeCvGrayToIndexed8(gray);
}

bool isTiffPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "tif" || suffix == "tiff";
}

QImage decodeWithOpenCv(const QString &path)
{
    const cv::Mat image = cv::imread(path.toStdString(), cv::IMREAD_ANYCOLOR | cv::IMREAD_ANYDEPTH);
    return decodeOpenCvImageToWorking(image);
}

QImage decodeWithQt(const QString &path, QString *errorString)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);

    if (!reader.canRead())
    {
        if (errorString)
            *errorString = reader.errorString();
        return QImage();
    }

    const QImage image = reader.read();
    if (image.isNull())
    {
        if (errorString)
            *errorString = reader.errorString();
        return QImage();
    }

    QImage working = decodeQtImageToWorking(image);
    if (working.isNull() && errorString)
        *errorString = QCoreApplication::translate("ImageManager", "The image format is not supported.");
    return working;
}

QImage loadWorkingImage(const QString &path, QString *errorString)
{
    QString qtError;

    if (isTiffPath(path))
    {
        QImage image = decodeWithOpenCv(path);
        if (!image.isNull())
            return image;

        image = decodeWithQt(path, &qtError);
        if (!image.isNull())
            return image;
    }
    else
    {
        QImage image = decodeWithQt(path, &qtError);
        if (!image.isNull())
            return image;

        image = decodeWithOpenCv(path);
        if (!image.isNull())
            return image;
    }

    if (errorString)
    {
        *errorString = qtError.isEmpty()
            ? QCoreApplication::translate("ImageManager", "The image could not be decoded.")
            : qtError;
    }
    return QImage();
}
}

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
    QString m_errorString;
public:
	MipMappedSource(const QString &path)
	{
        QImage image = loadWorkingImage(path, &m_errorString);
        if (!image.isNull())
        {
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

    QString errorString() const
    {
        return m_errorString;
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
        level = std::min(level, int(m_levels.size()));

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
	connect(m_watcher, &QFutureWatcher<ImageSource *>::finished, this, &ImageManager::onLoadFinished);
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
    m_loadingPath = path;

	emit status("Loading image...");

	QFuture<ImageSource *> future = QtConcurrent::run(&ImageManager::loadFunc, this, path);
	m_watcher->setFuture(future);
}

void ImageManager::clear()
{
    m_loadingPath.clear();
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
        QString message;
        if (source && source->tooBig())
            message = tr("The image is too large. It must be smaller than 32768x32768 pixels.");
        else if (source && !source->errorString().isEmpty())
            message = tr("Unable to load image \"%1\".\n\n%2")
                          .arg(QFileInfo(m_loadingPath).fileName(), source->errorString());
        else
            message = tr("Unable to load image \"%1\".").arg(QFileInfo(m_loadingPath).fileName());

        delete source;
        emit loadFailed(message);
	}

	emit status(QString());

	emit imageChanged();
}
