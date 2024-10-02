#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QtWidgets/QUndoStack>

class UndoManager : public QUndoStack
{
	Q_OBJECT
public:
	UndoManager(QObject *parent = nullptr);
	~UndoManager();

	static UndoManager *instance();

Q_SIGNALS:
	
private:
};

#endif
