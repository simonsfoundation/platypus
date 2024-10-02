#include <viewer.h>
#include <imageManager.h>
#include <project.h>
#include <polygon.h>
#include <manipulator.h>
#include <drawManipulator.h>
#include <editManipulator.h>
#include <maskManipulator.h>
#include <removeManipulator.h>
#include <textureManipulator.h>
#include <QtGui/QPainter>
#include <QtGui/QWheelEvent>
#include <QtCore/QDebug>

static QPoint s_downPos;
static QPoint s_lastPos;

Viewer::Viewer(QWidget *parent) : ImageViewer(parent)
{
	connect(&ImageManager::get(), SIGNAL(imageChanged()), SLOT(onImageChanged()));
	connect(Project::activeProject(), SIGNAL(changed()), SLOT(update()));
	connect(Project::activeProject(), SIGNAL(selectionChanged()), SLOT(update()));

    m_manipulator = nullptr;
	m_overlay = true;
	m_tool = kTool_None;
    m_action = kAction_None;
	m_handleSize = 11.0f;
}

Viewer::~Viewer()
{
    setManipulator(nullptr);
}

void Viewer::paintEvent(QPaintEvent *event)
{
	ImageViewer::paintEvent(event);
    QPainter p(this);

	if (m_overlay)
	{
		p.setTransform(m_xform);

		if (m_manipulator)
			m_manipulator->draw(this, p);
	}
}

void Viewer::resizeEvent(QResizeEvent *event)
{
	ImageViewer::resizeEvent(event);
	updateZoomRange();
}

void Viewer::setManipulator(Manipulator *m)
{
    if (m != m_manipulator)
    {
        delete m_manipulator;
        m_manipulator = m;
        if (m)
		{
            m->setParent(this);
			ImageSource *source = m->source();
			if (source)
				setSource(source);
		}
    }
}

void Viewer::onImageChanged()
{
	setSource(ImageManager::get().source());
	updateZoomRange();
	zoomToFit();
}

void Viewer::updateZoomRange()
{
	const ImageSource *source = ImageManager::get().source();
	if (source)
	{
		// calculate ideal zoom range
		QSize imageSize = source->size();
		QSize viewSize = size();

		float x_scale = float(viewSize.width()) / float(imageSize.width());
		float y_scale = float(viewSize.height()) / float(imageSize.height());
		float minZoom = std::min(x_scale, y_scale) * 0.9f;

		setZoomRange(minZoom, 1.0f);
	}
}

void Viewer::enterEvent(QEvent *event)
{
	ImageViewer::enterEvent(event);
}

bool Viewer::manipulatorShouldReceiveEvents() const
{
    return m_manipulator && (m_overlay || m_manipulator->receiveEventsWhenDisabled());
}

void Viewer::mousePressEvent(QMouseEvent *event)
{
	ImageViewer::mousePressEvent(event);
	if (!event->isAccepted())
	{
		if (m_action == kAction_None)
		{
			s_downPos = event->pos();

            if (manipulatorShouldReceiveEvents())
			{
				m_manipulator->mousePressEvent(this, event);
				if (event->isAccepted())
				{
					m_action = kAction_Manipulator;
				}
			}
		}
	}
}

void Viewer::mouseMoveEvent(QMouseEvent *event)
{
	ImageViewer::mouseMoveEvent(event);
	if (!event->isAccepted())
	{
        if (manipulatorShouldReceiveEvents())
        {
            switch (m_action)
            {
                case kAction_None:
                    m_manipulator->mouseHoverEvent(this, event);
                    break;
                case kAction_Manipulator:
                    m_manipulator->mouseMoveEvent(this, event);
                    break;
            }
        }
		s_lastPos = event->pos();
	}
}

void Viewer::mouseReleaseEvent(QMouseEvent *event)
{
	ImageViewer::mouseReleaseEvent(event);
	if (!event->isAccepted())
	{
		switch (m_action)
		{
			case kAction_None:
				break;
			case kAction_Manipulator:
                if (manipulatorShouldReceiveEvents())
					m_manipulator->mouseReleaseEvent(this, event);
				break;
		}
		m_action = kAction_None;
	}
}

void Viewer::keyPressEvent(QKeyEvent *event)
{
    ImageViewer::keyPressEvent(event);
	if (!event->isAccepted())
	{
        if (manipulatorShouldReceiveEvents())
		{
			m_manipulator->keyPressEvent(this, event);
		}
	}
}

void Viewer::keyReleaseEvent(QKeyEvent *event)
{
    ImageViewer::keyReleaseEvent(event);
	if (!event->isAccepted())
	{
        if (manipulatorShouldReceiveEvents())
		{
			m_manipulator->keyReleaseEvent(this, event);
		}
	}
}


// SLOTS
void Viewer::enableOverlay(bool state)
{
	m_overlay = state;
	update();
}

void Viewer::setTool(Tool tool)
{
	m_tool = tool;
    switch (tool)
    {
        case kTool_None:
            setManipulator(nullptr);
            break;
        case kTool_Polygon:
            setManipulator(new EditManipulator());
            break;
        case kTool_Mask:
            setManipulator(new MaskManipulator());
            break;
        case kTool_Remove:
            setManipulator(new RemoveManipulator());
            break;
        case kTool_Texture:
            setManipulator(new TextureManipulator());
            break;
    }
	update();
	emit toolChanged();
}

void Viewer::updateView()
{
    ImageViewer::updateView();

    // set up transform to be in full image coordinates
    const QSize &imageSize = getImageSize();
	const QRect &destRect = getDestRect();

    float x_scale = destRect.width() / float(imageSize.width());
    float y_scale = destRect.height() / float(imageSize.height());

    m_xform = QTransform::fromScale(x_scale, y_scale) * QTransform::fromTranslate(destRect.left(), destRect.top());
}

bool Viewer::hitPoint(const QPointF &pos, const QPointF &mouse) const
{
	return QRectF(pos.x() - m_handleSize / 2.0f, pos.y() - m_handleSize / 2.0f, m_handleSize, m_handleSize).contains(mouse);
}
