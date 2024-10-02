#include <viewerScroller.h>
#include <viewer.h>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>

ViewerScroller::ViewerScroller(Viewer *viewer) : m_viewer(viewer)
{
	viewer->setParent(this);

	m_hScroll = new QScrollBar(Qt::Horizontal, this);
	m_vScroll = new QScrollBar(Qt::Vertical, this);

	connect(m_hScroll, SIGNAL(valueChanged(int)), SLOT(onSlider(int)));
	connect(m_vScroll, SIGNAL(valueChanged(int)), SLOT(onSlider(int)));

	connect(m_viewer, SIGNAL(viewChanged()), SLOT(updateScrollBars()));
}

ViewerScroller::~ViewerScroller(void)
{
}

void ViewerScroller::resizeEvent(QResizeEvent *event)
{
	QSize size = this->size();

    int sbSize = style()->pixelMetric(QStyle::PM_ScrollBarExtent);

	m_hScroll->setGeometry(QRect(0, size.height() - sbSize, size.width(), sbSize));
	m_vScroll->setGeometry(QRect(size.width() - sbSize, 0, sbSize, size.height()));
	m_viewer->setGeometry(QRect(0, 0, size.width() - sbSize, size.height() - sbSize));
	updateScrollBars();

	QWidget::resizeEvent(event);
}

void ViewerScroller::updateScrollBars()
{
	const QRect &visRect = m_viewer->getVisibleSourceRect();
	const QSize &size = m_viewer->getImageSize();

	m_hScroll->blockSignals(true);
	m_vScroll->blockSignals(true);

	m_hScroll->setPageStep(visRect.width());
	m_hScroll->setRange(0, size.width() - visRect.width());
	m_hScroll->setValue(visRect.left());

	m_vScroll->setPageStep(visRect.height());
	m_vScroll->setRange(0, size.height() - visRect.height());
	m_vScroll->setValue(visRect.top());

	m_hScroll->blockSignals(false);
	m_vScroll->blockSignals(false);
}

void ViewerScroller::onSlider(int v)
{
	m_viewer->scroll(m_hScroll->value(), m_vScroll->value());
}

