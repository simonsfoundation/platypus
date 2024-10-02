#include <imageViewer.h>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtGui/QPixmap>
#include <QtCore/QDebug>

ImageViewer::ImageViewer(QWidget *parent) : QWidget(parent)
{
	m_source = NULL;
	m_zoom = 1.0f;
	m_margin = QSize(10, 10);
	m_defaultSize = QSize(1000, 800);
	m_updateImage = false;
	m_pan = false;
	m_panMode = false;
	m_space = false;
	m_minZoom = 0.1f;
	m_maxZoom = 10.0f;
	m_wheelEnabled = true;

	m_sourceSize = QSize(0, 0);
	m_destSize = QSize(0, 0);
	m_offset = QPointF(0.0f, 0.0f);
	m_panCursor.setShape(Qt::OpenHandCursor);
	m_panningCursor.setShape(Qt::ClosedHandCursor);
	m_showPanCursor = false;

	setMouseTracking(true);
	setBackgroundRole(QPalette::Base);

    QPalette p = palette();
    p.setColor(QPalette::Base, Qt::darkGray);
    setPalette(p);
}

void ImageViewer::setDefaultSize(const QSize &size)
{
	m_defaultSize = size;
}

void ImageViewer::setPanCursor(const QCursor &cursor)
{
	m_panCursor = cursor;
}

QSize ImageViewer::sizeHint() const
{
	return m_defaultSize;
}

const ImageSource *ImageViewer::getSource() const
{
	return m_source;
}

float ImageViewer::getZoom() const
{
	return m_zoom;
}

const QRect &ImageViewer::getVisibleSourceRect() const
{
	return m_visSourceRect;
}

const QSize &ImageViewer::getImageSize() const
{
	return m_sourceSize;
}

const QRect &ImageViewer::getDestRect() const
{
	return m_destRect;
}

const QRect &ImageViewer::getVisibleDestRect() const
{
	return m_visDestRect;
}

const QSize &ImageViewer::getDestSize() const
{
	return m_destSize;
}

const QSize &ImageViewer::getMargin() const
{
	return m_margin;
}

float ImageViewer::getMinZoom() const
{
	return m_minZoom;
}

float ImageViewer::getMaxZoom() const
{
	return m_maxZoom;
}

void ImageViewer::setZoomRange(float minZ, float maxZ)
{
	Q_ASSERT(maxZ >= minZ);

	m_minZoom = minZ;
	m_maxZoom = maxZ;

	if (m_zoom < m_minZoom)
		zoom(m_minZoom);
	else if (m_zoom > m_maxZoom)
		zoom(m_maxZoom);
}

void ImageViewer::enableWheel(bool state)
{
	m_wheelEnabled = state;
}

void ImageViewer::setMargin(const QSize &size)
{
	if (size != m_margin)
	{
		m_margin = size;
		updateView();
	}
}

void ImageViewer::setSource(const ImageSource *source)
{
	if (m_source)
		disconnect(m_source, SIGNAL(changed()), this, SLOT(updateImage()));
	m_source = source;
	if (source)
	{
		connect(m_source, SIGNAL(changed()), SLOT(updateImage()));
		connect(m_source, SIGNAL(destroyed(QObject *)), SLOT(onSourceDestroyed(QObject *)));
		m_sourceSize = source->size();
	}
	else
		m_sourceSize = QSize(0, 0);

	setEnabled(m_source != NULL);
	updateImage();
}

void ImageViewer::onSourceDestroyed(QObject *o)
{
	if (o == m_source)
	{
		m_source = nullptr;
		setSource(nullptr);
	}
}

void ImageViewer::updateImage()
{
	m_updateImage = true;
	updateView();
}

void ImageViewer::zoomIn()
{
	float zoom_scale = 1.25f;
	float zoom = m_zoom * zoom_scale;

	// stop at 100%
	if (m_zoom < 1.0f && zoom > 1.0f)
		zoom = 1.0f;

	ImageViewer::zoom(zoom);
}

void ImageViewer::zoomOut()
{
	float zoom_scale = 1.25f;
	float zoom = m_zoom / zoom_scale;

	// stop at 100%
	if (m_zoom > 1.0f && zoom < 1.0f)
		zoom = 1.0f;

	if (zoom < m_minZoom)
		zoom = m_minZoom;

	// can't zoom out past half the viewable area
	int minWidth = (this->width() - m_margin.width() * 2) / 2;
	int minHeight = (this->height() - m_margin.height() * 2) / 2;

	float width = m_sourceSize.width() * zoom;
	float height = m_sourceSize.height() * zoom;
	if (width < minWidth || height < minHeight)
	{
		width = minWidth;
		height = minHeight;
		float x_scale = width / m_sourceSize.width();
		float y_scale = height / m_sourceSize.height();
		zoom = x_scale < y_scale ? x_scale : y_scale;
	}
	ImageViewer::zoom(zoom);
}

void ImageViewer::zoomToFit()
{
	// calculate optimal zoom level
	if (m_source)
	{
		QSize availSize = size() - (m_margin * 2);

		float x_scale = (float)availSize.width() / (float)m_sourceSize.width();
		float y_scale =  (float)availSize.height() / (float)m_sourceSize.height();

		m_offset = QPointF(0, 0);
		zoom(std::min(x_scale, y_scale));
	}
}

void ImageViewer::zoom100()
{
	zoom(1.0f);
}

void ImageViewer::zoom(float zoom)
{
	if (zoom < m_minZoom)
		zoom = m_minZoom;
	else if (zoom > m_maxZoom)
		zoom = m_maxZoom;

	if (zoom != m_zoom)
	{
		m_zoom = zoom;
		updateView();
		emit zoomChanged();
	}
}

void ImageViewer::zoom(const QPoint &viewPt, float new_zoom)
{
	// zoom around the specified point
	zoom(new_zoom);
}

QPixmap ImageViewer::convertImage(QImage image) const
{
	return QPixmap::fromImage(image);
}

void ImageViewer::paintEvent(QPaintEvent *event)
{
	QPainter p(this);
	p.fillRect(rect(), palette().base());

	if (m_source)
	{
		if (m_updateImage)
		{
			QImage image = m_source->getTile(m_visSourceRect, m_visDestRect.size());

			if (image.isNull())
			{
				// no image returned, force another update event
				update();
			}
			else
			{
				// convert the tile to a pixmap
				m_pixmap = convertImage(image);
				m_updateImage = false;
			}
		}
		if (!m_pixmap.isNull())
			drawImage(p);
	}
}

void ImageViewer::drawImage(QPainter &p)
{
	p.drawPixmap(m_visDestRect.left(), m_visDestRect.top(), m_pixmap);
}

void ImageViewer::setVisibleSourceRect(const QRect &rect)
{
	m_visSourceRect = rect;
}

void ImageViewer::setVisibleDestRect(const QRect &rect)
{
	if (rect != m_visDestRect)
	{
		m_visDestRect = rect;
		m_updateImage = true;
	}
}

void ImageViewer::setDestRect(const QRect &rect)
{
	if (rect != m_destRect)
	{
		m_destRect = rect;
		m_updateImage = true;
	}
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	updateView();
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (event->button() == Qt::MiddleButton)
	{
		zoomToFit();
		event->accept();
	}
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
	// if we're allready panning, eat the event
	if (panning())
	{
		event->accept();
		return;
	}

	if (m_panMode || event->button() == Qt::MiddleButton)
	{
		if (!m_panMode && !m_space)
		{
			if (!m_showPanCursor)
			{
				m_prevCursor = cursor();
				m_showPanCursor = true;
				setCursor(m_panningCursor);
			}
		}
		else if (m_panMode)
		{
			setCursor(m_panningCursor);
		}
		m_downPos = event->pos();
		m_panRect = QRect(QPoint(m_visDestRect.left() - m_destRect.left(), m_visDestRect.top() - m_destRect.top()), m_visDestRect.size());
		beginPan();
		event->accept();
	}
	else
		event->ignore();
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
	if (m_pan && (event->button() == Qt::MiddleButton || m_panMode))
	{
		if (!m_panMode && !m_space)
		{
			setCursor(m_prevCursor);
			m_showPanCursor = false;
		}
		else if (m_panMode)
			setCursor(m_panCursor);

		endPan();
		event->accept();
	}
	else
		event->ignore();
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
	setFocus();

	if (m_pan)
	{
		QPoint delta = event->pos() - m_downPos;
		QPoint offset = m_panRect.topLeft() - delta;
		scroll(offset.x(), offset.y());
		event->accept();
		setCursor(m_panCursor);
	}
	else
		event->ignore();
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
	if (m_wheelEnabled)
	{
		double delta = (double)event->delta() / 120.0 / 10.0;
		double new_zoom = m_zoom + (delta * m_zoom);

		if (m_zoom < 1.0f && new_zoom > 1.0f)
			new_zoom = 1.0f;
		else if (m_zoom > 1.0f && new_zoom < 1.0f)
			new_zoom = 1.0f;

		event->accept();

		zoom(event->pos(), new_zoom);
	}
}

void ImageViewer::keyPressEvent(QKeyEvent *event)
{
	event->ignore();
	switch (event->key())
	{
		case Qt::Key_Space:
			if (!m_panMode)
			{
				m_space = true;
				m_panMode = true;
				if (!m_showPanCursor)
				{
					m_prevCursor = cursor();
					setCursor(m_panCursor);
					m_showPanCursor = true;
				}
				event->accept();
			}
			break;
	}
}

void ImageViewer::keyReleaseEvent(QKeyEvent *event)
{
	event->ignore();
	switch (event->key())
	{
		case Qt::Key_Space:
			if (m_space)
			{
				m_space = false;
				m_panMode = false;
				if (m_showPanCursor)
				{
					setCursor(m_prevCursor);
					m_showPanCursor = false;
				}
				event->accept();
			}
			break;
	}
}

void ImageViewer::scroll(int x, int y)
{
	// get the size of the visible region in source pixels
	QSize size = m_visDestRect.size();

	// do some bounds checking
	if (x < 0)
		x = 0;
	else if (x + size.width() > m_destSize.width())
		x = m_destSize.width() - size.width();
	if (y < 0)
		y = 0;
	else if (y + size.height() > m_destSize.height())
		y = m_destSize.height() - size.height();

	// generate a new visible source region
	QRectF destRect(QPoint(x, y), size);

	// calculate the offset
	QPointF center = destRect.center();
	QPointF scenter(center.x() / float(m_destSize.width()), center.y() / float(m_destSize.height()));

	QPointF offset = m_offset;

	QRect subRect(QPoint(m_visDestRect.left() - m_destRect.left(), m_visDestRect.top() - m_destRect.top()), size);

	if (x != subRect.left())
		offset.setX(0.5f - scenter.x());
	if (y != subRect.top())
		offset.setY(0.5f - scenter.y());

	if (offset != m_offset)
	{
		m_offset = offset;
		updateView();
	}
}

void ImageViewer::centerView(const QPoint &srcPt)
{
	// get current center
	QPointF center = srcPt;

	center.setX((center.x() - m_destRect.left()) / m_destRect.width());
	center.setY((center.y() - m_destRect.top()) / m_destRect.height());

	QPointF pos = QPointF(0.5f, 0.5f) - center;

	m_offset = pos;

	updateView();
}

void ImageViewer::updateView()
{
	// calculate destination size based on source size and zoom factor
	QSize destSize = QSize(int(m_sourceSize.width() * m_zoom + 0.5f), int(m_sourceSize.height() * m_zoom + 0.5f));

	QRect srcRect(QPoint(0, 0), m_sourceSize);
	QRect destRect(QPoint(0, 0), destSize);

	// calculate destination rectangle based on center value
	QPoint center = rect().center();
	destRect.translate(-destRect.center());
	destRect.translate(center);

	// now tack on offset
	QPoint offset(int(m_offset.x() * destSize.width() + 0.5f), int(m_offset.y() * destSize.height() + 0.5f));
	destRect.translate(offset);

	m_destRect = destRect;

	// calculate the visible portion of the destination rectangle and target size
	QRect visDestRect = m_destRect.intersected(rect());

	// use the visible portion of the destination rectangle to calculate the source tile needed
	int sx = int((visDestRect.left() - m_destRect.left()) / m_zoom + 0.5f);
	int sy = int((visDestRect.top() - m_destRect.top()) / m_zoom + 0.5f);
	int sw, sh;

	// special cases where entire image is visible
	if (visDestRect.width() >= m_destRect.width())
		sw = m_sourceSize.width();
	else
		sw = int(visDestRect.width() / m_zoom + 0.5f);

	if (visDestRect.height() >= m_destRect.height())
		sh = m_sourceSize.height();
	else
		sh = int(visDestRect.height() / m_zoom + 0.5f);

	QRect visSrcRect = QRect(sx, sy, sw, sh).intersected(srcRect);
	m_visDestRect = visDestRect;

	if (visSrcRect != m_visSourceRect || destSize != m_destSize)
	{
		m_destSize = destSize;
		m_visSourceRect = visSrcRect;
		m_updateImage = true;
	}
	emit viewChanged();
	update();
}

QPointF ImageViewer::viewToSource(const QPoint &pos) const
{
	double x = double(pos.x() - m_destRect.left()) / m_destRect.width() * m_sourceSize.width();
	double y = double(pos.y() - m_destRect.top()) / m_destRect.height() * m_sourceSize.height();

	return QPointF(x, y);
}

QPointF ImageViewer::sourceToView(const QPointF &pos) const
{
	double x = (pos.x() / m_sourceSize.width()) * m_destRect.width() + m_destRect.left();
	double y = (pos.y() / m_sourceSize.height()) * m_destRect.height() + m_destRect.top();

	return QPointF(x, y);

}

void ImageViewer::setPanMode(bool state)
{
	m_panMode = state;
	if (state)
		setCursor(m_panCursor);
	else
		setCursor(QCursor());
}

void ImageViewer::beginPan()
{
	Q_ASSERT(!panning());
	m_pan = true;
}

void ImageViewer::endPan()
{
	Q_ASSERT(panning());
	m_pan = false;
}

bool ImageViewer::panning() const
{
	return m_pan;
}

