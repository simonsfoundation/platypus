#ifndef VIEWERSCROLLER_H
#define VIEWERSCROLLER_H

#include <QtWidgets/QWidget>

class ViewerScroller : public QWidget
{
	Q_OBJECT
public:
	ViewerScroller(class Viewer *viewer);
	~ViewerScroller();

protected:
	virtual void resizeEvent(QResizeEvent *event);

protected slots:
	void updateScrollBars();
	void onSlider(int v);

private:
	class Viewer *m_viewer;
	class QScrollBar *m_hScroll;
	class QScrollBar *m_vScroll;
};

#endif
