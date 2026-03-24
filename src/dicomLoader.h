#ifndef DICOMLOADER_H
#define DICOMLOADER_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QImage>

struct DicomSliceInfo
{
    QString filePath;
    int frameIndex = 0;
    int instanceNumber = -1;
    bool hasSortPosition = false;
    double sortPosition = 0.0;
    QString label;
    QString detailText;
};

struct DicomSeriesInfo
{
    QString uid;
    QString modality;
    QString seriesDescription;
    QString label;
    QString detailText;
    QVector<DicomSliceInfo> slices;
};

struct DicomFileInfo
{
    bool isDicom = false;
    QString label;
    QString detailText;
    QVector<DicomSliceInfo> slices;
    QString errorString;
};

struct DicomDirectoryInfo
{
    QVector<DicomSeriesInfo> series;
    QString errorString;
};

struct DicomRenderResult
{
    QImage image;
    QString displayName;
    QString errorString;
};

bool isDicomFilePath(const QString &path);
DicomFileInfo inspectDicomFile(const QString &path);
DicomDirectoryInfo inspectDicomDirectory(const QString &directoryPath);
DicomRenderResult renderDicomSlice(const DicomSliceInfo &slice);

#endif
