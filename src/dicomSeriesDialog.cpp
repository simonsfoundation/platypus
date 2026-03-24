#include <dicomSeriesDialog.h>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

DicomSeriesDialog::DicomSeriesDialog(const QVector<DicomSeriesInfo> &series, QWidget *parent)
    : QDialog(parent), m_series(series), m_list(new QListWidget(this)), m_details(new QLabel(this))
{
    setWindowTitle(tr("Select DICOM Series"));
    resize(520, 360);

    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(
        tr("Multiple DICOM series were found. Choose which series to open."),
        this);
    heading->setWordWrap(true);
    layout->addWidget(heading);

    for (const DicomSeriesInfo &entry : m_series)
    {
        auto *item = new QListWidgetItem(entry.label, m_list);
        item->setToolTip(entry.detailText);
    }
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    m_details->setWordWrap(true);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_details);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_series.isEmpty());
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { updateSelectionDetails(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { accept(); });

    if (!m_series.isEmpty())
        m_list->setCurrentRow(0);
    updateSelectionDetails();
}

int DicomSeriesDialog::selectedIndex() const
{
    return m_list->currentRow();
}

void DicomSeriesDialog::updateSelectionDetails()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_series.size())
    {
        m_details->clear();
        return;
    }
    m_details->setText(m_series.at(row).detailText);
}
