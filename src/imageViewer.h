#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

#include <imageSource.h>
#include <QtWidgets/QWidget>
#include <QtGui/QPixmap>

class ImageViewer : public QWidget
{
	Q_OBJECT

public:
	ImageViewer(QWidget *parent = NULL);

	const ImageSource *getSource() const;

	virtual float getZoom() const;
	virtual float getMinZoom() const;
	virtual float getMaxZoom() const;

	const QRect &getVisibleSourceRect() const;		// the visible region in source pixels
	const QSize &getImageSize() const;				// get the entire image size in source pixels
	const QRect &getVisibleDestRect() const;		// the target rectangle in viewer coordinates
	const QRect &getDestRect() const;				// the target rectangle in viewer coordinates
	const QSize &getDestSize() const;
	const QSize &getMargin() const;

	QPointF viewToSource(const QPoint &pos) const;
	QPointF sourceToView(const QPointF &pos) const;

	bool panMode() const { return m_panMode; }
	bool wheelEnabled() const { return m_wheelEnabled; }

	virtual void beginPan();
	virtual void endPan();
	bool panning() const;

public:	// QWidget
	virtual QSize sizeHint() const;

public slots:
	virtual void setSource(const ImageSource *source);
	void zoomIn();
	void zoomOut();
	void zoomToFit();
	void zoom100();
	void zoom(float zoom);
	void zoom(const QPoint &viewPt, float zoom);
	void setPanMode(bool state);
	void scroll(int x, int y);
	void centerView(const QPoint &srcPt);
	void setZoomRange(float minZ, float maxZ);
	void enableWheel(bool state);
	void setMargin(const QSize &margin);

Q_SIGNALS:
	void viewChanged();
	void zoomChanged();

protected:
	virtual void paintEvent(QPaintEvent *event);
	virtual void resizeEvent(QResizeEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void wheelEvent(QWheelEvent *event);
	virtual void keyPressEvent(QKeyEvent *event);
	virtual void keyReleaseEvent(QKeyEvent *event);
	virtual void mouseDoubleClickEvent(QMouseEvent *event);

	virtual QPixmap convertImage(QImage image) const;

protected:
	void setPanCursor(const QCursor &cursor);
	void setDefaultSize(const QSize &size);

	virtual void updateView();
	virtual void drawImage(class QPainter &p);

	void setVisibleSourceRect(const QRect &rect);
	void setVisibleDestRect(const QRect &rect);
	void setDestRect(const QRect &rect);

protected slots:
	void updateImage();
	void onSourceDestroyed(QObject *o);

private:
	QSize m_defaultSize;
	const ImageSource *m_source;
	QSize m_margin;
	float m_minZoom;
	float m_maxZoom;
	float m_zoom;
	QSize m_sourceSize;				// source size in pixels
	QSize m_destSize;				// destination size (after scaling)
	QRect m_visSourceRect;			// visible region in source pixels
	QRect m_destRect;				// destination rectangle of entire zoomed image
	QRect m_visDestRect;			// visible destination rectangle
	QPointF m_offset;
	QPixmap m_pixmap;
	bool m_updateImage;
	bool m_pan;
	bool m_panMode;
	bool m_space;
	bool m_showPanCursor;
	bool m_wheelEnabled;

	QPoint m_downPos;
	QRect m_panRect;
	QCursor m_panCursor;
	QCursor m_panningCursor;
	QCursor m_prevCursor;

private:
};

#endif

