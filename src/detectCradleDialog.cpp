#include <detectCradleDialog.h>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QFrame>
#include <QtCore/QSettings>

static const char *kKey_Horizontal = "/detect/h";
static const char *kKey_Vertical = "/detect/v";
static const char *kKey_Mode = "/detect/mode";

namespace
{
QColor blendColors(const QColor &a, const QColor &b, qreal factor)
{
    const qreal inverse = 1.0 - factor;
    return QColor::fromRgbF(a.redF() * inverse + b.redF() * factor,
                            a.greenF() * inverse + b.greenF() * factor,
                            a.blueF() * inverse + b.blueF() * factor,
                            a.alphaF() * inverse + b.alphaF() * factor);
}

QString cssColor(const QColor &color)
{
    return color.name(QColor::HexArgb);
}

QString dialogStyleSheet(const QPalette &palette)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::WindowText);
    const QColor highlight = palette.color(QPalette::Highlight);
    const bool dark = window.lightness() < 140;

    const QColor sectionFill = blendColors(base, window, dark ? 0.18 : 0.08);
    const QColor sectionBorder = blendColors(text, window, dark ? 0.88 : 0.94);
    const QColor secondaryText = blendColors(text, window, dark ? 0.34 : 0.48);
    QColor accentFill = highlight;
    accentFill.setAlpha(dark ? 72 : 42);

    return QStringLiteral(R"(
QDialog#detectCradleDialog {
    background: %1;
}

QFrame#dialogSection {
    background: %2;
    border: 1px solid %3;
    border-radius: 12px;
}

QLabel#dialogSectionTitle {
    color: %4;
    font-size: 11px;
    font-weight: 700;
}

QLabel#dialogHint {
    color: %5;
}

QSpinBox {
    min-width: 68px;
    padding: 3px 8px;
    border: 1px solid %3;
    border-radius: 8px;
    background: %1;
}

QPushButton {
    min-width: 104px;
    padding: 5px 14px;
    border: 1px solid %3;
    border-radius: 9px;
    background: transparent;
}

QPushButton[role="primary"] {
    background: %6;
    border-color: %7;
}
)")
        .arg(cssColor(window),
             cssColor(sectionFill),
             cssColor(sectionBorder),
             cssColor(secondaryText),
             cssColor(secondaryText),
             cssColor(accentFill),
             cssColor(highlight));
}

void applyMacCompactSize(QWidget *widget)
{
#if defined(Q_OS_MACOS)
    widget->setAttribute(Qt::WA_MacSmallSize);
#else
    Q_UNUSED(widget);
#endif
}

QFrame *createSection(const QString &title, QVBoxLayout *&contentLayout, QWidget *parent)
{
    QFrame *section = new QFrame(parent);
    section->setObjectName("dialogSection");

    QVBoxLayout *outer = new QVBoxLayout(section);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(10);

    QLabel *titleLabel = new QLabel(title, section);
    titleLabel->setObjectName("dialogSectionTitle");
    outer->addWidget(titleLabel);

    contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(8);
    outer->addLayout(contentLayout);

    return section;
}
}

DetectCradleDialog::DetectCradleDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName("detectCradleDialog");
    setWindowTitle(tr("Detect Cradle"));
    setModal(true);
    setMinimumWidth(420);
    setStyleSheet(dialogStyleSheet(palette()));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    QVBoxLayout *modeLayout = nullptr;
    layout->addWidget(createSection(tr("Detection Mode"), modeLayout, this));

    m_auto = new QRadioButton(tr("Auto Detect"));
    m_assisted = new QRadioButton(tr("Assisted Detect"));
    applyMacCompactSize(m_auto);
    applyMacCompactSize(m_assisted);
    modeLayout->addWidget(m_auto);
    modeLayout->addWidget(m_assisted);

    m_group = new QButtonGroup(this);
    m_group->addButton(m_auto, 0);
    m_group->addButton(m_assisted, 1);
    connect(m_group, &QButtonGroup::idClicked, this, &DetectCradleDialog::onMode);

    QVBoxLayout *membersContent = nullptr;
    layout->addWidget(createSection(tr("Guide Counts"), membersContent, this));

    QLabel *hint = new QLabel(tr("Only used for assisted detection."), this);
    hint->setObjectName("dialogHint");
    membersContent->addWidget(hint);

    QLabel *hLabel = new QLabel(tr("Horizontal Members"));
    QLabel *vLabel = new QLabel(tr("Vertical Members"));
    m_h = new QSpinBox;
    m_v = new QSpinBox;

    applyMacCompactSize(m_h);
    applyMacCompactSize(m_v);
    m_h->setRange(1, 50);
    m_v->setRange(1, 50);
    m_h->setAlignment(Qt::AlignRight);
    m_v->setAlignment(Qt::AlignRight);

    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->addWidget(hLabel, 0, 0);
    grid->addWidget(m_h, 0, 1);
    grid->addWidget(vLabel, 1, 0);
    grid->addWidget(m_v, 1, 1);
    grid->setColumnStretch(0, 1);
    m_layout = grid;

    membersContent->addLayout(grid);
    membersContent->addStretch(1);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &DetectCradleDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DetectCradleDialog::reject);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Detect"));
    buttons->button(QDialogButtonBox::Ok)->setProperty("role", "primary");
    applyMacCompactSize(buttons->button(QDialogButtonBox::Ok));
    applyMacCompactSize(buttons->button(QDialogButtonBox::Cancel));

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
