#ifndef DICOMSLICEDIALOG_H
#define DICOMSLICEDIALOG_H

#include <dicomLoader.h>
#include <QtWidgets/QDialog>

class QListWidget;
class QLabel;

class DicomSliceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DicomSliceDialog(const QString &title,
                              const QVector<DicomSliceInfo> &slices,
                              QWidget *parent = nullptr);

    int selectedIndex() const;

private:
    void updatePreview();

    QVector<DicomSliceInfo> m_slices;
    QListWidget *m_list;
    QLabel *m_details;
    QLabel *m_preview;
};

#endif
