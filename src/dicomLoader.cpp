#include <dicomLoader.h>

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#if __has_include(<dcmtk/dcmjpls/djdecode.h>)
#include <dcmtk/dcmjpls/djdecode.h>
#define PLATYPUS_HAVE_DCMTK_JPEGLS 1
#endif
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QRegularExpression>
#include <QtGui/QVector3D>

#include <algorithm>
#include <mutex>

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

QString conditionText(const OFCondition &condition)
{
    return QString::fromLocal8Bit(condition.text());
}

QString readString(DcmDataset *dataset, const DcmTagKey &key)
{
    OFString value;
    if (dataset && dataset->findAndGetOFString(key, value).good())
        return QString::fromLatin1(value.c_str()).trimmed();
    return QString();
}

bool readSint32(DcmDataset *dataset, const DcmTagKey &key, int *value)
{
    Sint32 dcmtkValue = 0;
    if (dataset && dataset->findAndGetSint32(key, dcmtkValue).good())
    {
        if (value)
            *value = int(dcmtkValue);
        return true;
    }
    return false;
}

bool readFloat64Values(DcmDataset *dataset,
                       const DcmTagKey &key,
                       int count,
                       double *values)
{
    if (!dataset)
        return false;

    for (int i = 0; i < count; ++i)
    {
        Float64 value = 0.0;
        if (dataset->findAndGetFloat64(key, value, static_cast<unsigned long>(i)).bad())
            return false;
        values[i] = value;
    }
    return true;
}

QString fileDisplayName(const QString &path)
{
    return QFileInfo(path).fileName();
}

QString formatSliceLabel(int frameIndex, int instanceNumber, int totalFrames)
{
    if (totalFrames > 1)
        return QStringLiteral("Frame %1").arg(frameIndex + 1);
    if (instanceNumber >= 0)
        return QStringLiteral("Slice %1").arg(instanceNumber);
    return QStringLiteral("Slice %1").arg(frameIndex + 1);
}

QString formatSliceDetail(const QString &fileName,
                          int width,
                          int height,
                          int instanceNumber,
                          int totalFrames)
{
    QStringList parts;
    parts << fileName;
    if (instanceNumber >= 0 && totalFrames <= 1)
        parts << QStringLiteral("Instance %1").arg(instanceNumber);
    else if (totalFrames > 1)
        parts << QStringLiteral("%1 frame%2").arg(totalFrames).arg(totalFrames == 1 ? QString() : QStringLiteral("s"));
    if (width > 0 && height > 0)
        parts << QStringLiteral("%1x%2").arg(width).arg(height);
    return parts.join(QStringLiteral(" · "));
}

QString formatSeriesLabel(const QString &modality,
                          const QString &seriesDescription,
                          const QString &uid,
                          int sliceCount)
{
    QStringList parts;
    if (!modality.isEmpty())
        parts << modality;
    if (!seriesDescription.isEmpty())
        parts << seriesDescription;
    if (parts.isEmpty())
        parts << (uid.isEmpty() ? QStringLiteral("DICOM Series") : uid);
    return QStringLiteral("%1 (%2 slice%3)")
        .arg(parts.join(QStringLiteral(" · ")))
        .arg(sliceCount)
        .arg(sliceCount == 1 ? QString() : QStringLiteral("s"));
}

QString formatSeriesDetail(const QString &uid, const QString &directoryPath)
{
    QStringList parts;
    if (!uid.isEmpty())
        parts << uid;
    parts << directoryPath;
    return parts.join(QStringLiteral("\n"));
}

QByteArray encodePath(const QString &path)
{
    return QFile::encodeName(path);
}

void registerDicomCodecs()
{
    static std::once_flag codecsRegistered;
    std::call_once(codecsRegistered, [] {
        DJDecoderRegistration::registerCodecs();
        DcmRLEDecoderRegistration::registerCodecs();
#if defined(PLATYPUS_HAVE_DCMTK_JPEGLS)
        DJLSDecoderRegistration::registerCodecs();
#endif
    });
}

bool buildSliceSortPosition(DcmDataset *dataset, double *sortPosition)
{
    double orientation[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double position[3] = {0.0, 0.0, 0.0};
    if (!readFloat64Values(dataset, DCM_ImageOrientationPatient, 6, orientation) ||
        !readFloat64Values(dataset, DCM_ImagePositionPatient, 3, position))
        return false;

    const QVector3D row{float(orientation[0]), float(orientation[1]), float(orientation[2])};
    const QVector3D column{float(orientation[3]), float(orientation[4]), float(orientation[5])};
    const QVector3D normal = QVector3D::crossProduct(row, column);
    if (normal.lengthSquared() <= 0.0f)
        return false;

    const QVector3D imagePosition{float(position[0]), float(position[1]), float(position[2])};
    if (sortPosition)
        *sortPosition = double(QVector3D::dotProduct(imagePosition, normal));
    return true;
}

struct DicomFileScanResult
{
    bool isDicom = false;
    QString errorString;
    QString seriesUid;
    QString seriesDescription;
    QString modality;
    QVector<DicomSliceInfo> slices;
};

DicomFileScanResult scanDicomFile(const QString &path)
{
    DicomFileScanResult result;
    registerDicomCodecs();

    DcmFileFormat fileFormat;
    const QByteArray encoded = encodePath(path);
    OFCondition loadStatus = fileFormat.loadFile(encoded.constData());
    if (loadStatus.bad())
    {
        result.errorString = conditionText(loadStatus);
        return result;
    }

    result.isDicom = true;
    DcmDataset *dataset = fileFormat.getDataset();
    result.seriesUid = readString(dataset, DCM_SeriesInstanceUID);
    result.seriesDescription = readString(dataset, DCM_SeriesDescription);
    result.modality = readString(dataset, DCM_Modality);

    DicomImage image(OFFilename(encoded.constData()));
    if (image.getStatus() != EIS_Normal)
    {
        result.errorString = QString::fromLatin1(DicomImage::getString(image.getStatus()));
        return result;
    }

    const int width = int(image.getWidth());
    const int height = int(image.getHeight());
    const int frameCount = int(std::max<unsigned long>(1, image.getNumberOfFrames()));

    int instanceNumber = -1;
    readSint32(dataset, DCM_InstanceNumber, &instanceNumber);

    double sortPosition = 0.0;
    const bool hasSortPosition = buildSliceSortPosition(dataset, &sortPosition);
    const QString fileName = fileDisplayName(path);

    result.slices.reserve(frameCount);
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        DicomSliceInfo slice;
        slice.filePath = path;
        slice.frameIndex = frameIndex;
        slice.instanceNumber = instanceNumber;
        slice.hasSortPosition = hasSortPosition;
        slice.sortPosition = sortPosition;
        slice.label = formatSliceLabel(frameIndex, instanceNumber, frameCount);
        slice.detailText = formatSliceDetail(fileName, width, height, instanceNumber, frameCount);
        result.slices.push_back(slice);
    }

    return result;
}

void sortSlices(QVector<DicomSliceInfo> *slices)
{
    if (!slices)
        return;

    const bool allHaveSortPosition = std::all_of(
        slices->cbegin(), slices->cend(),
        [](const DicomSliceInfo &slice) { return slice.hasSortPosition; });

    const bool allHaveInstance = std::all_of(
        slices->cbegin(), slices->cend(),
        [](const DicomSliceInfo &slice) { return slice.instanceNumber >= 0; });

    std::stable_sort(slices->begin(), slices->end(),
                     [allHaveSortPosition, allHaveInstance](const DicomSliceInfo &a,
                                                            const DicomSliceInfo &b) {
        if (allHaveSortPosition && a.sortPosition != b.sortPosition)
            return a.sortPosition < b.sortPosition;
        if (allHaveInstance && a.instanceNumber != b.instanceNumber)
            return a.instanceNumber < b.instanceNumber;
        if (a.filePath != b.filePath)
            return a.filePath < b.filePath;
        return a.frameIndex < b.frameIndex;
    });
}

int defaultSliceIndex(const QVector<DicomSliceInfo> &slices)
{
    if (slices.isEmpty())
        return -1;
    return slices.size() / 2;
}
}

bool isDicomFilePath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("dcm") || suffix == QStringLiteral("dicom");
}

DicomFileInfo inspectDicomFile(const QString &path)
{
    DicomFileInfo info;
    const DicomFileScanResult scan = scanDicomFile(path);
    info.isDicom = scan.isDicom;
    info.errorString = scan.errorString;
    info.slices = scan.slices;

    if (!scan.isDicom || !scan.errorString.isEmpty())
        return info;

    if (scan.slices.size() <= 1)
    {
        info.label = fileDisplayName(path);
        info.detailText = scan.slices.isEmpty() ? QString() : scan.slices.first().detailText;
    }
    else
    {
        info.label = QStringLiteral("%1 (%2 frames)")
                         .arg(fileDisplayName(path))
                         .arg(scan.slices.size());
        info.detailText = QStringLiteral("Select a frame from this DICOM object.");
    }
    return info;
}

DicomDirectoryInfo inspectDicomDirectory(const QString &directoryPath)
{
    DicomDirectoryInfo info;
    QDir directory(directoryPath);
    const QFileInfoList entries = directory.entryInfoList(
        QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    if (entries.isEmpty())
    {
        info.errorString = QStringLiteral("No top-level files were found in \"%1\".")
                               .arg(QDir::toNativeSeparators(directoryPath));
        return info;
    }

    QHash<QString, int> seriesIndexByUid;
    for (const QFileInfo &entry : entries)
    {
        const DicomFileScanResult scan = scanDicomFile(entry.absoluteFilePath());
        if (!scan.isDicom)
            continue;
        if (!scan.errorString.isEmpty() || scan.slices.isEmpty())
            continue;

        const QString uid = scan.seriesUid.isEmpty()
            ? QStringLiteral("__missing_series_uid__")
            : scan.seriesUid;
        int index = seriesIndexByUid.value(uid, -1);
        if (index < 0)
        {
            DicomSeriesInfo series;
            series.uid = scan.seriesUid;
            series.modality = scan.modality;
            series.seriesDescription = scan.seriesDescription;
            series.label = formatSeriesLabel(scan.modality, scan.seriesDescription, scan.seriesUid, 0);
            series.detailText = formatSeriesDetail(scan.seriesUid, QDir::toNativeSeparators(directoryPath));
            info.series.push_back(series);
            index = info.series.size() - 1;
            seriesIndexByUid.insert(uid, index);
        }

        auto &series = info.series[index];
        for (const DicomSliceInfo &slice : scan.slices)
            series.slices.push_back(slice);
    }

    if (info.series.isEmpty())
    {
        info.errorString = QStringLiteral("No top-level DICOM images were found in \"%1\".")
                               .arg(QDir::toNativeSeparators(directoryPath));
        return info;
    }

    for (DicomSeriesInfo &series : info.series)
    {
        sortSlices(&series.slices);
        series.label = formatSeriesLabel(series.modality, series.seriesDescription, series.uid, series.slices.size());
        if (!series.slices.isEmpty())
        {
            const QString fileName = fileDisplayName(series.slices.at(defaultSliceIndex(series.slices)).filePath);
            QStringList parts;
            parts << QStringLiteral("%1 slice%2")
                         .arg(series.slices.size())
                         .arg(series.slices.size() == 1 ? QString() : QStringLiteral("s"));
            parts << fileName;
            if (!series.uid.isEmpty())
                parts << series.uid;
            series.detailText = parts.join(QStringLiteral(" · "));
        }
    }

    std::stable_sort(info.series.begin(), info.series.end(),
                     [](const DicomSeriesInfo &a, const DicomSeriesInfo &b) {
        return a.label < b.label;
    });
    return info;
}

DicomRenderResult renderDicomSlice(const DicomSliceInfo &slice)
{
    DicomRenderResult result;
    registerDicomCodecs();

    const QByteArray encoded = encodePath(slice.filePath);
    DicomImage image(OFFilename(encoded.constData()));
    if (image.getStatus() != EIS_Normal)
    {
        result.errorString = QString::fromLatin1(DicomImage::getString(image.getStatus()));
        return result;
    }

    const unsigned long totalFrames = image.getNumberOfFrames();
    if (slice.frameIndex < 0 || (totalFrames > 0 && static_cast<unsigned long>(slice.frameIndex) >= totalFrames))
    {
        result.errorString = QStringLiteral("Requested DICOM frame %1 is out of range.")
                                 .arg(slice.frameIndex + 1);
        return result;
    }

    if (image.getWindowCount() > 0)
    {
        if (!image.setWindow(0))
            image.setMinMaxWindow();
    }
    else
    {
        image.setMinMaxWindow();
    }

    const int width = int(image.getWidth());
    const int height = int(image.getHeight());
    if (width <= 0 || height <= 0)
    {
        result.errorString = QStringLiteral("The DICOM image has invalid dimensions.");
        return result;
    }

    const void *buffer = image.getOutputData(8, static_cast<unsigned long>(slice.frameIndex));
    if (!buffer)
    {
        result.errorString = QString::fromLatin1(DicomImage::getString(image.getStatus()));
        return result;
    }

    QImage rendered = makeIndexed8Image(QSize(width, height));
    const uchar *sourceBytes = static_cast<const uchar *>(buffer);
    for (int y = 0; y < height; ++y)
        memcpy(rendered.scanLine(y), sourceBytes + (y * width), size_t(width));
    image.deleteOutputData();

    result.image = rendered;
    result.displayName = slice.label.isEmpty() ? fileDisplayName(slice.filePath) : slice.label;
    return result;
}
