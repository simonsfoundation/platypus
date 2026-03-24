#include <dicomSliceDialog.h>

#include <QtCore/QSignalBlocker>
#include <QtGui/QPixmap>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

DicomSliceDialog::DicomSliceDialog(const QString &title,
                                   const QVector<DicomSliceInfo> &slices,
                                   QWidget *parent)
    : QDialog(parent),
      m_slices(slices),
      m_list(new QListWidget(this)),
      m_details(new QLabel(this)),
      m_preview(new QLabel(this))
{
    setWindowTitle(title);
    resize(820, 520);

    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(
        tr("Select the slice to load into the current 2D editing workflow."),
        this);
    heading->setWordWrap(true);
    layout->addWidget(heading);

    auto *content = new QHBoxLayout;
    layout->addLayout(content, 1);

    for (const DicomSliceInfo &slice : m_slices)
    {
        auto *item = new QListWidgetItem(slice.label, m_list);
        item->setToolTip(slice.detailText);
    }
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setMinimumWidth(220);
    content->addWidget(m_list, 0);

    auto *previewColumn = new QVBoxLayout;
    content->addLayout(previewColumn, 1);

    m_preview->setMinimumSize(360, 360);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    m_preview->setText(tr("Preview unavailable"));
    previewColumn->addWidget(m_preview, 1);

    m_details->setWordWrap(true);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewColumn->addWidget(m_details);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_slices.isEmpty());
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) { updatePreview(); });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { accept(); });

    if (!m_slices.isEmpty())
        m_list->setCurrentRow(m_slices.size() / 2);
    updatePreview();
}

int DicomSliceDialog::selectedIndex() const
{
    return m_list->currentRow();
}

void DicomSliceDialog::updatePreview()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_slices.size())
    {
        m_preview->setText(tr("Preview unavailable"));
        m_preview->setPixmap(QPixmap());
        m_details->clear();
        return;
    }

    const DicomSliceInfo &slice = m_slices.at(row);
    m_details->setText(slice.detailText);

    const DicomRenderResult preview = renderDicomSlice(slice);
    if (preview.image.isNull())
    {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(preview.errorString.isEmpty()
                               ? tr("Preview unavailable")
                               : preview.errorString);
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(preview.image);
    m_preview->setPixmap(
        pixmap.scaled(m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_preview->setText(QString());
}
