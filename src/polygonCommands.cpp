#include <polygonCommands.h>
#include <project.h>

// AddPolygon
AddPolygonCommand::AddPolygonCommand(PolygonPointer p, QUndoCommand *parent) :
	QUndoCommand("Add Polygon", parent)
{
    m_polygons << p;
}

AddPolygonCommand::AddPolygonCommand(PolygonList p, QUndoCommand *parent) :
	QUndoCommand("Add Polygon", parent),
    m_polygons(p)
{
}

AddPolygonCommand::~AddPolygonCommand()
{
}

void AddPolygonCommand::undo()
{
    for (auto p : m_polygons)
        Project::activeProject()->removePolygon(p);
}

void AddPolygonCommand::redo()
{
    for (auto p : m_polygons)
        Project::activeProject()->addPolygon(p);
}

// RemovePolygon
RemovePolygonCommand::RemovePolygonCommand(PolygonPointer p, QUndoCommand *parent) :
	QUndoCommand("Remove Polygon", parent)
{
    m_polygons << p;
}

RemovePolygonCommand::RemovePolygonCommand(PolygonList p, QUndoCommand *parent) :
	QUndoCommand("Remove Polygon", parent),
    m_polygons(p)
{
}

RemovePolygonCommand::~RemovePolygonCommand()
{
}

void RemovePolygonCommand::undo()
{
    for (auto p : m_polygons)
        Project::activeProject()->addPolygon(p);
}

void RemovePolygonCommand::redo()
{
    for (auto p : m_polygons)
        Project::activeProject()->removePolygon(p);
}

// SelectPolygonsCommand
SelectPolygonsCommand::SelectPolygonsCommand(PolygonPointer p, bool state, QUndoCommand *parent) :
	QUndoCommand("Select Polygons", parent)
{
	if (p->isSelected() != state)
	{
		m_old = Project::activeProject()->selection();
		m_new = m_old;
		if (state)
			m_new << p;
		else
			m_new.removeAll(p);
	}
}

SelectPolygonsCommand::SelectPolygonsCommand(const QList<PolygonPointer> &list, bool state, QUndoCommand *parent) :
	QUndoCommand("Select Polygons", parent)
{
	m_old = Project::activeProject()->selection();
	m_new = m_old;
	for (auto p : list)
	{
		if (p->isSelected() != state)
		{
			if (state)
				m_new << p;
			else
				m_new.removeAll(p);
		}
	}
}

SelectPolygonsCommand::SelectPolygonsCommand(PolygonPointer p, QUndoCommand *parent) :
	QUndoCommand("Select Polygons", parent)
{
	m_old = Project::activeProject()->selection();
	m_new << p;
}

SelectPolygonsCommand::SelectPolygonsCommand(const QList<PolygonPointer> &list, QUndoCommand *parent) :
	QUndoCommand("Select Polygons", parent), m_new(list)
{
	m_old = Project::activeProject()->selection();
}

SelectPolygonsCommand::~SelectPolygonsCommand()
{
}

void SelectPolygonsCommand::undo()
{
	for (auto p : m_new)
		p->setSelected(false);
	for (auto p : m_old)
		p->setSelected(true);
}

void SelectPolygonsCommand::redo()
{
	for (auto p : m_old)
		p->setSelected(false);
	for (auto p : m_new)
		p->setSelected(true);
}

// EditPolygonsCommand
EditPolygonCommand::EditPolygonCommand(PolygonPointer p, const QString &key, const QVariant &value, QUndoCommand *parent) :
	QUndoCommand("Edit Polygon", parent),
	m_polygon(p),
	m_key(key),
	m_new(value)
{
	m_old = p->value(key);
}

EditPolygonCommand::EditPolygonCommand(PolygonPointer p, const Polygon::PointList &points, QUndoCommand *parent) :
	QUndoCommand("Edit Polygon", parent),
    m_polygon(p)
{
    m_oldPoints = p->points();
    m_newPoints = points;
}

EditPolygonCommand::~EditPolygonCommand()
{
}

void EditPolygonCommand::undo()
{
    if (m_key.isEmpty())
        m_polygon->set(m_oldPoints);
    else
        m_polygon->setValue(m_key, m_old);
}

void EditPolygonCommand::redo()
{
    if (m_key.isEmpty())
        m_polygon->set(m_newPoints);
    else
        m_polygon->setValue(m_key, m_new);
}
