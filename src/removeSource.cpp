#include <removeSource.h>
#include <imageManager.h>
#include <project.h>
#include <polygon.h>
#include <cmath>

RemoveSource::RemoveSource(QObject *parent) : ImageSource(parent), m_bypass(false), m_project(nullptr), m_final(false)
{
}

RemoveSource::~RemoveSource()
{
}

void RemoveSource::setProject(Project *project)
{
    if (project != m_project)
    {
        if (m_project)
            m_project->disconnect(SIGNAL(changed()), this, SLOT(invalidate()));
        m_project = project;
        if (m_project)
            connect(m_project, SIGNAL(changed()), this, SLOT(invalidate()));
    }
}

void RemoveSource::setBypass(bool bypass)
{
	if (bypass != m_bypass)
	{
		m_bypass = bypass;
		invalidate();
	}
}

void RemoveSource::setIsFinal(bool final)
{
	if (final != m_final)
	{
		m_final = final;
		invalidate();
	}
}

QSize RemoveSource::size() const
{
	return ImageManager::get().source()->size();
}

QString RemoveSource::name() const
{
	return "Result";
}

bool RemoveSource::isValid() const
{
	return ImageManager::get().source()->isValid();
}

QImage RemoveSource::getTile(const QRect &tile, const QSize &size) const
{
    if (m_bypass)
        return ImageManager::get().source()->getTile(tile, size);

    const cvImageRef &cradle = ImageManager::get().resultImage();

	// crop out tile
	cv::Rect roi(tile.left(), tile.top(), tile.width(), tile.height());

	// convert result to 8 bit
	cv::Mat converted(tile.height(), tile.width(), CV_8U);
	cv::Mat cradleWrap(tile.height(), tile.width(), CV_32F, (void *)iplImageRow<float>(cradle, tile.top(), tile.left()), cradle->widthStep);
	cv::convertScaleAbs(cradleWrap, converted);

	// resize
	QImage result(size, QImage::Format_Indexed8);
    result.setColorCount(256);
    for (int i = 0; i < 256; i++)
        result.setColor(i, qRgba(i, i, i, 255));

	cv::Mat resultWrap(result.height(), result.width(), CV_8U, result.bits(), result.bytesPerLine());
	cv::resize(converted, resultWrap, resultWrap.size());

    if (m_final)
        return result;

	QImage source = ImageManager::get().source()->getTile(tile, size);
	const cvImageRef &removeMask = ImageManager::get().removeMask();

    // generate intensity mask
    QImage ppScaled(size, QImage::Format_ARGB32);
    auto polygons = Project::activeProject()->polygons(Polygon::OUTPUT);
    const QRgb defPixel = qRgba(0, 127, 255, 0);
    if (polygons.empty())
    {
        ppScaled.fill(defPixel);
    }
    else
    {
        std::vector<QRgb> table(polygons.size());
        std::fill(table.begin(), table.end(), defPixel);
        for (auto p : polygons)
        {
            int index = p->value("index").toInt();
			if (index >= 0)
			{
				int black = p->value("black").toInt();
				int gamma = p->value("gamma").toInt();
				int white = p->value("white").toInt();
				uchar median = p->value("median").toInt();

                table[index] = qRgba(black, gamma + 127, white, median);
			}
        }
        QImage mask(tile.size(), QImage::Format_ARGB32);
        for (int y = tile.top(); y < tile.top() + tile.height(); y++)
        {
            const unsigned short *srow = iplImageRow<unsigned short>(removeMask, y, tile.left());
            QRgb *mrow = (QRgb *)mask.scanLine(y - tile.top());
            const QRgb *end = mrow + tile.width();
            while (mrow != end)
            {
                unsigned short v = *srow;
                *mrow = v == 0 ? defPixel : table[v - 1];
                srow++;
                mrow++;
            }
        }
        // scale the mask
        {
            cv::Mat input(mask.height(), mask.width(), CV_8UC4, mask.bits(), mask.bytesPerLine());
            cv::Mat output(ppScaled.height(), ppScaled.width(), CV_8UC4, ppScaled.bits(), ppScaled.bytesPerLine());
            cv::resize(input, output, output.size(), cv::INTER_NEAREST);
        }
    }

	// subtract result from source
	for (int y = 0; y < result.height(); y++)
	{
		const uchar *srow = source.scanLine(y);
        const QRgb *mrow = (const QRgb *)ppScaled.scanLine(y);
		uchar *drow = result.scanLine(y);
		const uchar *end = drow + result.width();
		while (drow != end)
		{
            float black = qRed(*mrow) / 255.0f;
            float gamma = float(qGreen(*mrow) - 127) / 255.0f + 1.0f;
            float white = qBlue(*mrow) / 255.0f;
            float median = qAlpha(*mrow) / 255.0f;
            median = 0.0f;

            //#define LevelsControlInputRange(color, minInput, maxInput) min(max(color – vec3(minInput), vec3(0.0)) / (vec3(maxInput) – vec3(minInput)), vec3(1.0))

            // pow(color, vec3(1.0 / gamma))
            // result =  ((original- cradle_image) - M) * contrast + M + intensity

            float v = float(*srow - *drow) / 255.0f - median;
            v = v < 0.0f ? 0.0f : v;
            v = (v - black) / (white - black);
            v += median;
            v = v < 0.0f ? 0.0f : v;
            v = std::pow(v, gamma);

            *drow = v > 1.0 ? 255 : uchar(v * 255.0f);
			drow++;
			srow++;
            mrow++;
		}
	}
	return result;
}

