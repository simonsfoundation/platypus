#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <QtCore/QObject>

class Clipboard : public QObject
{
	Q_OBJECT
public:
	Clipboard(QObject *parent = nullptr);
	~Clipboard();

	static Clipboard *instance();

	const QObjectList &objects() const;
	bool hasType(const QMetaObject *type) const;

Q_SIGNALS:
	void changed();

public slots:
	void set(const QObjectList &objects);
	void clear();
	
private:
	QObjectList m_objects;
};

#endif
