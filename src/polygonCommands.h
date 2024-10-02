#ifndef POLYGONCOMMANDS_H
#define POLYGONCOMMANDS_H

#include <polygon.h>
#include <QtWidgets/QUndoCommand>

class AddPolygonCommand : public QUndoCommand
{
public:
	AddPolygonCommand(PolygonPointer p, QUndoCommand *parent = nullptr);
	AddPolygonCommand(PolygonList p, QUndoCommand *parent = nullptr);
	~AddPolygonCommand();

	virtual void undo();
	virtual void redo();

protected:

private:
    PolygonList m_polygons;
};

class RemovePolygonCommand : public QUndoCommand
{
public:
	RemovePolygonCommand(PolygonPointer p, QUndoCommand *parent = nullptr);
	RemovePolygonCommand(PolygonList p, QUndoCommand *parent = nullptr);
	~RemovePolygonCommand();

	virtual void undo();
	virtual void redo();

protected:

private:
    PolygonList m_polygons;
};

class SelectPolygonsCommand : public QUndoCommand
{
public:
	SelectPolygonsCommand(PolygonPointer p, bool state, QUndoCommand *parent = nullptr);
	SelectPolygonsCommand(const QList<PolygonPointer> &list, bool state, QUndoCommand *parent = nullptr);

	SelectPolygonsCommand(PolygonPointer p, QUndoCommand *parent = nullptr);
	SelectPolygonsCommand(const QList<PolygonPointer> &list, QUndoCommand *parent = nullptr);
	~SelectPolygonsCommand();

	virtual void undo();
	virtual void redo();

private:
    PolygonList m_old;
    PolygonList m_new;
};

class EditPolygonCommand : public QUndoCommand
{
public:
	EditPolygonCommand(PolygonPointer p, const QString &key, const QVariant &value, QUndoCommand *parent = nullptr);
	EditPolygonCommand(PolygonPointer p, const Polygon::PointList &points, QUndoCommand *parent = nullptr);
	~EditPolygonCommand();

	virtual void undo();
	virtual void redo();

protected:

private:
    PolygonPointer m_polygon;
	QString m_key;
	QVariant m_old;
	QVariant m_new;
    Polygon::PointList m_oldPoints;
    Polygon::PointList m_newPoints;
};

#endif
