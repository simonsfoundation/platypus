#include <maskCommands.h>
#include <project.h>

// AddMaskShape
AddMaskShapeCommand::AddMaskShapeCommand(MaskShapePointer p, QUndoCommand *parent) :
	QUndoCommand("Add Shape", parent),
	m_shape(p)
{
}

AddMaskShapeCommand::~AddMaskShapeCommand()
{
}

void AddMaskShapeCommand::undo()
{
	//Project::activeProject()->mask()->removeShape(m_shape);
}

void AddMaskShapeCommand::redo()
{
	//Project::activeProject()->mask()->addShape(m_shape);
}

// RemoveMaskShape
RemoveMaskShapeCommand::RemoveMaskShapeCommand(MaskShapePointer p, QUndoCommand *parent) :
	QUndoCommand("Remove Shape", parent),
	m_shape(p)
{
}

RemoveMaskShapeCommand::~RemoveMaskShapeCommand()
{
}

void RemoveMaskShapeCommand::undo()
{
	//Project::activeProject()->mask()->addShape(m_shape);
}

void RemoveMaskShapeCommand::redo()
{
	//Project::activeProject()->mask()->removeShape(m_shape);
}

// EditMaskShapeCommand
EditMaskShapeCommand::EditMaskShapeCommand(MaskShapePointer p, int index, const MaskShape::Point &pt, QUndoCommand *parent) :
	QUndoCommand("Edit Shape", parent),
	m_shape(p),
	m_index(index),
	m_newPoint(pt)
{
	m_oldPoint = p->point(index);
}

EditMaskShapeCommand::EditMaskShapeCommand(MaskShapePointer p, const QString &key, const QVariant &value, QUndoCommand *parent) :
	QUndoCommand("Edit Shape", parent),
	m_shape(p),
	m_index(-1),
	m_key(key),
	m_newValue(value)
{
	m_oldValue = p->get(m_key);
}

EditMaskShapeCommand::~EditMaskShapeCommand()
{
}

void EditMaskShapeCommand::undo()
{
	if (m_key.isEmpty() && m_index >= 0)
		m_shape->set(m_index, m_oldPoint);
	else
		m_shape->set(m_key, m_oldValue);
}

void EditMaskShapeCommand::redo()
{
	if (m_key.isEmpty() && m_index >= 0)
		m_shape->set(m_index, m_newPoint);
	else
		m_shape->set(m_key, m_newValue);
}

// SetMaskShapeCommand
SetMaskShapeCommand::SetMaskShapeCommand(MaskShapePointer p, const MaskShape::PointList &points, QUndoCommand *parent) :
	QUndoCommand("Edit Shape", parent),
	m_shape(p),
	m_oldPoints(p->points()),
	m_newPoints(points)
{
}

void SetMaskShapeCommand::undo()
{
	m_shape->set(m_oldPoints);
}

void SetMaskShapeCommand::redo()
{
	m_shape->set(m_newPoints);
}
