#include <slider.h>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtGui/QValidator>
#include <QtGui/QKeyEvent>

static bool s_editing = false;

Slider::Slider(QWidget *parent) : QWidget(parent), m_indeterminate(false)
{
    m_label = new QLabel;
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setFixedWidth(128);
    m_edit = new QLineEdit;
    m_edit->setReadOnly(false);
    m_edit->setFixedWidth(40);
    m_edit->setAlignment(Qt::AlignRight);

    connect(m_slider, SIGNAL(sliderPressed()), SLOT(onSliderPressed()));
    connect(m_slider, SIGNAL(sliderReleased()), SLOT(onSliderReleased()));
    connect(m_slider, SIGNAL(valueChanged(int)), SLOT(onValue(int)));
    connect(m_edit, SIGNAL(editingFinished()), SLOT(onEdit()));

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setMargin(0);
    layout->addWidget(m_label);
    layout->addWidget(m_slider);
    layout->addWidget(m_edit);

    m_label->hide();

    m_label->installEventFilter(this);
    m_slider->installEventFilter(this);
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
        dynamic_cast<QWidget *>(target)->setFocus();
    return QWidget::eventFilter(target, event);
}
