#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QUndoStack>

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
