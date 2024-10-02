#ifndef MASKCOMMANDS_H
#define MASKCOMMANDS_H

#include <mask.h>
#include <QtWidgets/QUndoCommand>
#include <QtCore/QVariant>

class AddMaskShapeCommand : public QUndoCommand
{
public:
	AddMaskShapeCommand(MaskShapePointer  p, QUndoCommand *parent = nullptr);
	~AddMaskShapeCommand();

	virtual void undo();
	virtual void redo();

protected:

private:
    MaskShapePointer m_shape;
};

class RemoveMaskShapeCommand : public QUndoCommand
{
public:
	RemoveMaskShapeCommand(MaskShapePointer p, QUndoCommand *parent = nullptr);
	~RemoveMaskShapeCommand();

	virtual void undo();
	virtual void redo();

protected:

private:
    MaskShapePointer m_shape;
};

class EditMaskShapeCommand : public QUndoCommand
{
public:
	EditMaskShapeCommand(MaskShapePointer p, int index, const MaskShape::Point &pt, QUndoCommand *parent = nullptr);
	EditMaskShapeCommand(MaskShapePointer p, const QString &key, const QVariant &value, QUndoCommand *parent = nullptr);
	~EditMaskShapeCommand();

	virtual void undo();
	virtual void redo();

protected:

private:
  MaskShapePointer m_shape;
	int m_index;
	MaskShape::Point m_oldPoint;
	MaskShape::Point m_newPoint;
	QString m_key;
	QVariant m_oldValue;
	QVariant m_newValue;
};

class SetMaskShapeCommand : public QUndoCommand
{
public:
	SetMaskShapeCommand(MaskShapePointer p, const MaskShape::PointList &points, QUndoCommand *parent = nullptr);

	virtual void undo();
	virtual void redo();

protected:

private:
  MaskShapePointer m_shape;
	MaskShape::PointList m_oldPoints;
	MaskShape::PointList m_newPoints;
};

#endif
