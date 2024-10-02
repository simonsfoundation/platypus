#include <detectCradleDialog.h>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtCore/QSettings>

static const char *kKey_Horizontal = "/detect/h";
static const char *kKey_Vertical = "/detect/v";
static const char *kKey_Mode = "/detect/mode";

DetectCradleDialog::DetectCradleDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Detect Cradle"));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QRadioButton *autoDetect = new QRadioButton(tr("Auto Detect"));
    QRadioButton *assistedDetect = new QRadioButton(tr("Assisted Detect"));

    layout->addWidget(autoDetect);
    layout->addWidget(assistedDetect);

    m_group = new QButtonGroup(this);
    m_group->addButton(autoDetect, 0);
    m_group->addButton(assistedDetect, 1);
    connect(m_group, SIGNAL(buttonClicked(int)), SLOT(onMode(int)));

    QLabel *hLabel = new QLabel(tr("Horizontal Members"));
    QLabel *vLabel = new QLabel(tr("Vertical Members"));
    m_h = new QSpinBox;
    m_v = new QSpinBox;

    m_h->setRange(1, 50);
    m_v->setRange(1, 50);

    QGridLayout *grid = new QGridLayout;
    grid->setMargin(0);
    grid->setSpacing(4);
    grid->addWidget(hLabel, 0, 0);
    grid->addWidget(m_h, 0, 1);
    grid->addWidget(vLabel, 1, 0);
    grid->addWidget(m_v, 1, 1);
    m_layout = grid;

    layout->addLayout(grid);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, SIGNAL(accepted()), SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), SLOT(reject()));
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Detect"));

    QSettings settings;
    int h = settings.value(kKey_Horizontal, 5).toInt();
    int v = settings.value(kKey_Vertical, 5).toInt();
    m_h->setValue(h);
    m_v->setValue(v);

    int mode = settings.value(kKey_Mode, 0).toInt();
    m_group->button(mode)->setChecked(true);
    onMode(mode);
}

DetectCradleDialog::~DetectCradleDialog()
{
    QSettings settings;
    settings.setValue(kKey_Horizontal, m_h->value());
    settings.setValue(kKey_Vertical, m_v->value());
    settings.setValue(kKey_Mode, m_group->checkedId());
}

void DetectCradleDialog::onMode(int mode)
{
    for (int i = 0; i < m_layout->count(); i++)
        m_layout->itemAt(i)->widget()->setEnabled(mode == 1);
}

QSize DetectCradleDialog::members() const
{
    if (m_group->checkedId() == 1)
    {
        int h = m_h->value();
        int v = m_v->value();
        return QSize(h, v);
    }
    return QSize();
}
