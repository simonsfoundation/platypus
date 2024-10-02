#ifndef SLIDER_H
#define SLIDER_H

#include <QtWidgets/QWidget>
#include <QtCore/QVector>

class Slider : public QWidget
{
    Q_OBJECT
public:
    Slider(QWidget *parent = nullptr);
    
    void setRange(int min, int max);
    void setLabel(const QString &label);
    int value() const;
    
    int minimum() const;
    int maximum() const;
    
    bool isIndeterminate() const;
    bool isEditing() const;
    
Q_SIGNALS:
    void sliderPressed();
    void sliderReleased();
    void valueChanged(int value);

public slots:
    void setValue(int value);
    void setValues(const QVector<int> &values);

protected:
    virtual bool eventFilter(QObject *target, QEvent *event);
    
    void changeValue(int value);

private slots:
    void onSliderPressed();
    void onSliderReleased();
    void onValue(int);
    void onEdit();
    
private:
    class QLabel *m_label;
    class QSlider *m_slider;
    class QLineEdit *m_edit;
    bool m_indeterminate;
};

#endif