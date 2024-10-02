#include <clipboard.h>

static Clipboard *s_instance;

Clipboard *Clipboard::instance() { return s_instance; }

Clipboard::Clipboard(QObject *parent) : QObject(parent)
{
	s_instance = this;
}

Clipboard::~Clipboard()
{
	s_instance = nullptr;
}

const QObjectList &Clipboard::objects() const
{
	return m_objects;
}

bool Clipboard::hasType(const QMetaObject *type) const
{
	for (auto object : m_objects)
	{
		if (object->metaObject() == type)
			return true;
	}
	return false;
}

void Clipboard::set(const QObjectList &objects)
{
	// delete old objects
	for (auto object : m_objects)
		delete object;

	// set parents
	for (auto object : objects)
		object->setParent(this);

	m_objects = objects;
	emit changed();
}

void Clipboard::clear()
{
	set(QObjectList());
	emit changed();
}
