#ifndef REMOVESOURCE_H
#define REMOVESOURCE_H

#include <imageSource.h>
#include <cvImage.h>

class Project;

class RemoveSource : public ImageSource
{
Q_OBJECT
public:
	RemoveSource(QObject *parent = nullptr);
	~RemoveSource();

    void setProject(Project *project);

	virtual QSize size() const;
	virtual QString name() const;
	virtual bool isValid() const;
	virtual QImage getTile(const QRect &tile, const QSize &size) const;

	void setBypass(bool bypass);
    void setIsFinal(bool final);

    bool isFinal() const { return m_final; }

Q_SIGNALS:

public slots:

private:
    Project *m_project;
	bool m_bypass;
    bool m_final;

private slots:
};

#endif
