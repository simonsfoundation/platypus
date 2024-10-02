#include <manipulator.h>
#include <imageManager.h>
#include <viewer.h>
#include <QtGui/QKeyEvent>

Manipulator::Manipulator(QObject *parent) : QObject(parent)
{
}

Manipulator::~Manipulator()
{
}

bool Manipulator::receiveEventsWhenDisabled() const
{
    return false;
}

ImageSource *Manipulator::source() const
{
	return ImageManager::get().source();
}

void Manipulator::mouseHoverEvent(Viewer *viewer, QMouseEvent *event)
{
    event->setAccepted(false);
}

void Manipulator::mousePressEvent(Viewer *viewer, QMouseEvent *event)
{
    event->setAccepted(false);
}

void Manipulator::mouseMoveEvent(Viewer *viewer, QMouseEvent *event)
{
    event->setAccepted(false);
}

void Manipulator::mouseReleaseEvent(Viewer *viewer, QMouseEvent *event)
{
    event->setAccepted(false);
}

void Manipulator::keyPressEvent(Viewer *viewer, QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape)
	{
		cancel(viewer);
		event->accept();
	}
    else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down)
    {
        float dist = 1.0f / viewer->getZoom();
        switch (event->key())
        {
            case Qt::Key_Left:
                nudge(viewer, QPointF(-dist, 0.0f));
                break;
            case Qt::Key_Right:
                nudge(viewer, QPointF(dist, 0.0f));
                break;
            case Qt::Key_Up:
                nudge(viewer, QPointF(0.0f, -dist));
                break;
            case Qt::Key_Down:
                nudge(viewer, QPointF(0.0f, dist));
                break;
        }
    }
	else
		event->setAccepted(false);
}

void Manipulator::keyReleaseEvent(Viewer *viewer, QKeyEvent *event)
{
    event->setAccepted(false);
}

void Manipulator::draw(const Viewer *viewer, QPainter &p) const
{
}

void Manipulator::cancel(Viewer *viewer)
{
}

void Manipulator::nudge(Viewer *viewer, const QPointF &delta)
{
}

bool Manipulator::canDoCommand(const QString &cmdName) const
{
	return false;
}

void Manipulator::doCommand(const QString &cmdName)
{
}

bool Manipulator::pointOnLine(const QPointF &p0, const QPointF &p1, const QPointF &mouse_pt, float tolerance, float *t)
{
	float left = std::min(p0.x(), p1.x()) - tolerance;
	float top = std::min(p0.y(), p1.y()) - tolerance;
	float right = std::max(p0.x(), p1.x()) + tolerance;
	float bottom = std::max(p0.y(), p1.y()) + tolerance;

	QPointF p = mouse_pt;
	QRectF rect(left, top, right - left, bottom - top);
	if (rect.contains(p))
	{
		float delta = tolerance * tolerance;

		float px = mouse_pt.x();
		float py = mouse_pt.y();
		float ax = p0.x();
		float ay = p0.y();
		float bx = p1.x();
		float by = p1.y();

		float Px = px - ax;
		float Py = py - ay;
		float lenSP = Px * Px + Py * Py;
		if (lenSP < delta)
		{
			if (t)
				*t = 0.0f;
			return true;
		}

		float Lx = bx - ax;
		float Ly = by - ay;
		float lenSL = Lx * Lx + Ly * Ly;
		if (lenSP > lenSL)
		{
			Px -= -Lx;
			Py -= -Ly;
			if ((Px * Px + Py * Py) < delta)
			{
				if (t)
					*t = 1.0f;
				return true;
			}
			return false;
		}

		float norm = ::sqrt(lenSP / lenSL);
		Px -= (Lx * norm);
		Py -= (Ly * norm);
		if ((Px * Px + Py * Py) < delta)
		{
			if (t)
			{
				float ld = ::sqrtf((bx - ax) * (bx - ax) +
								 (by - ay) * (by - ay));
				float pd = ::sqrtf((mouse_pt.x() - ax) * (mouse_pt.x() - ax) +
								 (mouse_pt.y() - ay) * (mouse_pt.y() - ay));

				*t = pd / ld;
			}
			return true;
		}
	}
	return false;
}

void Manipulator::beginParameterChange(const QString &key)
{
}

void Manipulator::endParameterChange(const QString &key)
{
}
