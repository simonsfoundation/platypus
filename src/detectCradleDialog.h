#ifndef DETECTCRADLEDIALOG_H
#define DETECTCRADLEDIALOG_H

#include <QtWidgets/QDialog>

class DetectCradleDialog : public QDialog
{
	Q_OBJECT

public:
	DetectCradleDialog(QWidget *parent = nullptr);
	~DetectCradleDialog();
    
    QSize members() const;

protected:

private slots:
    void onMode(int);

private:
    class QRadioButton *m_auto;
    class QRadioButton *m_assisted;
    class QGridLayout *m_layout;
    class QSpinBox *m_h;
    class QSpinBox *m_v;
    class QButtonGroup *m_group;
};

#endif
