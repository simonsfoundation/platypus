#ifndef DICOMSERIESDIALOG_H
#define DICOMSERIESDIALOG_H

#include <dicomLoader.h>
#include <QtWidgets/QDialog>

class QListWidget;
class QLabel;

class DicomSeriesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DicomSeriesDialog(const QVector<DicomSeriesInfo> &series,
                               QWidget *parent = nullptr);

    int selectedIndex() const;

private:
    void updateSelectionDetails();

    QVector<DicomSeriesInfo> m_series;
    QListWidget *m_list;
    QLabel *m_details;
};

#endif
