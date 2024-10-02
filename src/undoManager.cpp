#include <undoManager.h>

static UndoManager *s_instance;

UndoManager *UndoManager::instance() { return s_instance; }

UndoManager::UndoManager(QObject *parent) : QUndoStack(parent)
{
	s_instance = this;
}

UndoManager::~UndoManager()
{
	s_instance = nullptr;
}
