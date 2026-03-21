#include <slider.h>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QApplication>
#include <QtGui/QValidator>
#include <QtGui/QKeyEvent>
#include <QtGui/QStatusTipEvent>
#include <QtWidgets/QSizePolicy>

static bool s_editing = false;

namespace
{
class HoverStatusTipFilter : public QObject
{
public:
    explicit HoverStatusTipFilter(QWidget *owner) : QObject(owner)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (!widget)
            return QObject::eventFilter(watched, event);

        if (event->type() == QEvent::Enter) {
            const QString statusTip = widget->statusTip();
            if (!statusTip.isEmpty()) {
                QStatusTipEvent statusEvent(statusTip);
                QApplication::sendEvent(widget->window(), &statusEvent);
            }
        } else if (event->type() == QEvent::Leave) {
            QStatusTipEvent clearEvent(QString{});
            QApplication::sendEvent(widget->window(), &clearEvent);
        }

        return QObject::eventFilter(watched, event);
    }
};

void installHoverStatusTip(QWidget *widget)
{
    widget->installEventFilter(new HoverStatusTipFilter(widget));
}
}

Slider::Slider(QWidget *parent) : QWidget(parent), m_indeterminate(false)
{
    m_label = new QLabel;
    m_label->setObjectName("toneSliderLabel");
    m_label->setMinimumWidth(46);
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setObjectName("toneSliderControl");
    m_slider->setFixedWidth(88);
    m_edit = new QLineEdit;
    m_edit->setObjectName("toneSliderValue");
    m_edit->setReadOnly(false);
    m_edit->setFixedWidth(36);
    m_edit->setAlignment(Qt::AlignRight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

#if defined(Q_OS_MACOS)
    setAttribute(Qt::WA_MacSmallSize);
    m_slider->setAttribute(Qt::WA_MacSmallSize);
    m_edit->setAttribute(Qt::WA_MacSmallSize);
#endif

    connect(m_slider, &QSlider::sliderPressed, this, &Slider::onSliderPressed);
    connect(m_slider, &QSlider::sliderReleased, this, &Slider::onSliderReleased);
    connect(m_slider, &QSlider::valueChanged, this, &Slider::onValue);
    connect(m_edit, &QLineEdit::editingFinished, this, &Slider::onEdit);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_label);
    layout->addWidget(m_slider);
    layout->addWidget(m_edit);

    m_label->hide();

    m_label->installEventFilter(this);
    m_slider->installEventFilter(this);
    m_edit->installEventFilter(this);
    installHoverStatusTip(m_label);
    installHoverStatusTip(m_slider);
    installHoverStatusTip(m_edit);
}

int Slider::minimum() const
{
    return m_slider->minimum();
}

int Slider::maximum() const
{
    return m_slider->maximum();
}

void Slider::setRange(int min, int max)
{
    m_slider->setRange(min, max);
    m_edit->setValidator(new QIntValidator(min, max));
}

void Slider::setLabel(const QString &label)
{
    m_label->setText(label);
    m_label->setVisible(!label.isEmpty());
}

void Slider::setHelpText(const QString &toolTip, const QString &statusTip)
{
    const QString effectiveStatus = statusTip.isEmpty() ? toolTip : statusTip;
    setToolTip(toolTip);
    setStatusTip(effectiveStatus);
    setWhatsThis(effectiveStatus);
    setAccessibleName(m_label->text().isEmpty() ? toolTip : m_label->text());
    setAccessibleDescription(effectiveStatus);

    for (QWidget *widget : {static_cast<QWidget *>(m_label),
                            static_cast<QWidget *>(m_slider),
                            static_cast<QWidget *>(m_edit)}) {
        widget->setToolTip(toolTip);
        widget->setStatusTip(effectiveStatus);
        widget->setWhatsThis(effectiveStatus);
        widget->setAccessibleDescription(effectiveStatus);
    }
}

int Slider::value() const
{
    return m_slider->value();
}

bool Slider::isEditing() const
{
    return s_editing;
}

bool Slider::isIndeterminate() const
{
    return m_indeterminate;
}

void Slider::setValue(int value)
{
    m_indeterminate = false;
    m_slider->setValue(value);
    m_edit->setText(QString::number(value));
}

void Slider::setValues(const QVector<int> &values)
{
    m_indeterminate = false;
    int value = 0;
    for (int i = 0; i < values.size(); i++)
    {
        if (i == 0)
            value = values[i];
        else if (value != values[i])
            m_indeterminate = true;
    }
    m_slider->blockSignals(true);
    if (m_indeterminate)
    {
        m_slider->setValue(0);
        m_edit->setText("???");
    }
    else
    {
        m_slider->setValue(value);
        if (values.empty())
            m_edit->setText(QString());
        else
            m_edit->setText(QString::number(value));
    }
    m_slider->blockSignals(false);
}

void Slider::onSliderPressed()
{
    s_editing = true;
    emit sliderPressed();
}

void Slider::onSliderReleased()
{
    s_editing = false;
    emit sliderReleased();
}

void Slider::onValue(int value)
{
    if (!s_editing)
        emit sliderPressed();
    if (m_indeterminate)
        m_edit->setText(value < 0 ? QString::number(value) : QString("+%1").arg(value));
    else
        m_edit->setText(QString::number(value));

    emit valueChanged(value);
    if (!s_editing)
        emit sliderReleased();
}

void Slider::changeValue(int value)
{
    if (value < minimum())
        value = minimum();
    else if (value > maximum())
        value = maximum();

    if (value != this->value())
    {
        emit sliderPressed();
        m_slider->blockSignals(true);
        setValue(value);
        m_slider->blockSignals(false);
        emit valueChanged(value);
        emit sliderReleased();
    }
}

void Slider::onEdit()
{
    int value = m_edit->text().toInt();
    changeValue(value);
}

bool Slider::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::Enter)
    {
        if (QWidget *widget = qobject_cast<QWidget *>(target))
            widget->setFocus();
    }
    return QWidget::eventFilter(target, event);
}
