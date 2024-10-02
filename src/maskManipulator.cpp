#include <maskManipulator.h>
#include <project.h>
#include <viewer.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <cmath>

#if 0
static MaskShapePointer findShape(const Viewer *viewer, const QPoint &mouse, int modifiers, int &index, MaskShape::Handle &handle)
{
	QPointF pos = viewer->viewToSource(mouse);
	auto shapes = Project::activeProject()->mask()->shapes();
	for (auto shape : shapes)
	{
		// check for over a control point
		auto points = shape->points();
		for (int i = 0; i < points.size(); i++)
		{
			QPointF knot = viewer->sourceToView(points[i].knot);
			QPointF tan_in = viewer->sourceToView(points[i].tan_in + points[i].knot);
			QPointF tan_out = viewer->sourceToView(points[i].tan_out + points[i].knot);

			if (viewer->hitPoint(knot, mouse) && !(modifiers & Qt::AltModifier))
			{
				handle = MaskShape::kHandle_Knot;
				index = i;
				return shape;
			}
			if (viewer->hitPoint(tan_out, mouse))
			{
				handle = MaskShape::kHandle_OutTangent;
				index = i;
				return shape;
			}
			if (viewer->hitPoint(tan_in, mouse))
			{
				handle = MaskShape::kHandle_InTangent;
				index = i;
				return shape;
			}
		}
	}

	handle = MaskShape::kHandle_None;
	index = -1;

	// now check for point over shape
	for (auto shape : shapes)
	{
		QPainterPath path = shape->path();
		if (path.contains(pos))
			return shape;
	}

	return MaskShapePointer();
}
#endif

MaskManipulator::MaskManipulator(QObject *parent) : EditManipulator(parent)
{
}

MaskManipulator::~MaskManipulator()
{
}

Polygon::Type MaskManipulator::polygonType() const
{
    return Polygon::MASK;
}

void MaskManipulator::drawMask(const Viewer *viewer, QPainter &p) const
{
}

#if 0
void MaskManipulator::mouseHoverEvent(Viewer *viewer, QMouseEvent *event)
{
	s_lastPos = event->pos();

	viewer->setCursor(QCursor());

	int index;
	MaskShape::Handle handle;
	MaskShapePointer shape = findShape(viewer, event->pos(), event->modifiers(), index, handle);
	if (shape)
	{
		if (index >= 0)
			viewer->setCursor(Qt::CrossCursor);
		else
			viewer->setCursor(Qt::PointingHandCursor);
	}
	else if (event->modifiers() == Qt::AltModifier)
	{
		if (pointOnShape(viewer->viewToSource(event->pos()).toPoint(), viewer->handleSize() / viewer->getZoom()))
		{
			viewer->setCursor(Qt::CrossCursor);
			event->accept();
			return;
		}
	}

	if (!event->isAccepted())
	    Manipulator::mouseHoverEvent(viewer, event);
}

void MaskManipulator::mousePressEvent(Viewer *viewer, QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
	{
		s_downPos = viewer->viewToSource(event->pos());
		s_clickPos = event->pos();

		if (event->modifiers() == Qt::AltModifier)
		{
			QPoint pos = viewer->viewToSource(event->pos()).toPoint();
			float t;
			m_edit.shape = pointOnShape(pos, viewer->handleSize() / viewer->getZoom(), &m_edit.t);
			if (m_edit.shape)
			{
				// save the old points
				m_edit.points = m_edit.shape->points();

				// insert the new one
				m_edit.index = m_edit.shape->insertPoint(m_edit.t);
				m_edit.handle = MaskShape::kHandle_Knot;
				m_edit.point = m_edit.shape->point(m_edit.index);

				m_mode = kMode_Drag;
				event->accept();
			}
		}

		if (m_mode == kMode_None)
		{
			// look for existing shape
			m_edit.index = -1;
			m_edit.shape = findShape(viewer, event->pos(), event->modifiers(), m_edit.index, m_edit.handle);
			if (m_edit.shape)
			{
				if (m_edit.index >= 0)
					m_edit.point = m_edit.shape->point(m_edit.index);
				event->accept();
			}
			else
			{
				// create a new one
				float handleSize = viewer->handleSize();
				QPoint pos = viewer->viewToSource(event->pos()).toPoint();
				m_rect = QRect(pos.x(), pos.y(), 0, 0);
				m_mode = kMode_Draw;
				event->accept();
			}
		}
	}
	if (!event->isAccepted())
		Manipulator::mousePressEvent(viewer, event);
}

static MaskShape::Point adjustPoint(MaskShape::Point point, MaskShape::Handle handle, const QPoint &delta, const QMouseEvent *event)
{
	switch (handle)
	{
		case MaskShape::kHandle_Knot:
			point.knot += delta;
			break;
		case MaskShape::kHandle_InTangent:
			point.tan_in += delta;
			point.tan_out -= delta;
			break;
		case MaskShape::kHandle_OutTangent:
			point.tan_out += delta;
			point.tan_in -= delta;
			break;
	}

	return point;
}

void MaskManipulator::mouseMoveEvent(Viewer *viewer, QMouseEvent *event)
{
	s_lastPos = event->pos();
	if (m_mode == kMode_None)
	{
		if ((event->pos() - s_clickPos).manhattanLength() > QApplication::startDragDistance())
		{
			if (event->modifiers() == 0)
			{
				if (m_edit.shape)
				{
					m_mode = kMode_Drag;
				}
			}
		}
	}

    switch (m_mode)
    {
        case kMode_Draw:
		{
			QPoint pos = viewer->viewToSource(event->pos()).toPoint();
			int left = pos.x() < s_downPos.x() ? pos.x() : s_downPos.x();
			int top = pos.y() < s_downPos.y() ? pos.y() : s_downPos.y();
			int right = pos.x() > s_downPos.x() ? pos.x() : s_downPos.x();
			int bottom = pos.y() > s_downPos.y() ? pos.y() : s_downPos.y();
			m_rect = QRect(QPoint(left, top), QPoint(right, bottom));
			viewer->update();
			event->accept();
            break;
		}
		case kMode_Drag:
		{
			if (m_edit.index >= 0)
			{
				QPoint pos = viewer->viewToSource(event->pos()).toPoint();
				QPoint delta = pos - s_downPos.toPoint();
				m_edit.shape->set(m_edit.index, adjustPoint(m_edit.point, m_edit.handle, delta, event));
				event->accept();
			}
			break;
		}
    }
	if (!event->isAccepted())
		Manipulator::mouseMoveEvent(viewer, event);
}

void MaskManipulator::mouseReleaseEvent(Viewer *viewer, QMouseEvent *event)
{
    switch (m_mode)
    {
        case kMode_Draw:
		{
            if (m_rect.isValid())
            {
				MaskShapePointer shape(new MaskShape());
				MaskShape::PointList points;
				points << MaskShape::Point(m_rect.topLeft());
				points << MaskShape::Point(m_rect.topRight());
				points << MaskShape::Point(m_rect.bottomRight());
				points << MaskShape::Point(m_rect.bottomLeft());
				shape->set(points);
				UndoManager::instance()->push(new AddMaskShapeCommand(shape));

				m_rect = QRect();
				viewer->update();
				event->accept();
            }
            break;
		}

		case kMode_Drag:
		{
			// restore the original point
			if (m_edit.index >= 0)
			{
				if (m_edit.points.empty())
				{
					m_edit.restore();
					QPoint pos = viewer->viewToSource(event->pos()).toPoint();
					QPoint delta = pos - s_downPos.toPoint();
					MaskShape::Point point = adjustPoint(m_edit.point, m_edit.handle, delta, event);
					UndoManager::instance()->push(new EditMaskShapeCommand(m_edit.shape, m_edit.index, point));
				}
				else
				{
					MaskShape::PointList points = m_edit.shape->points();
					m_edit.restore();
					UndoManager::instance()->push(new SetMaskShapeCommand(m_edit.shape, points));
				}
			}
			m_edit.clear();
			break;
		}
    }
	m_mode = kMode_None;
	if (!event->isAccepted())
		Manipulator::mouseReleaseEvent(viewer, event);
}

void MaskManipulator::keyPressEvent(Viewer *viewer, QKeyEvent *event)
{
	if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)
	{
		int index;
		MaskShape::Handle handle;
		MaskShapePointer shape = findShape(viewer, s_lastPos, event->modifiers(), index, handle);
		if (shape)
		{
			if (index >= 0)
			{
				// remove the point
				shape->deletePoint(index);
				if (shape->points().empty())
					UndoManager::instance()->push(new RemoveMaskShapeCommand(shape));
				else
					UndoManager::instance()->push(new SetMaskShapeCommand(shape, shape->points()));
			}
			else
			event->accept();
		}
	}

	if (!event->isAccepted())
		Manipulator::keyPressEvent(viewer, event);
}

void MaskManipulator::cancel(Viewer *viewer)
{
	if (m_mode != kMode_None)
	{
        if (m_mode == kMode_Draw)
        {
            m_rect = QRect();
            viewer->update();
        }
        else if (m_mode == kMode_Drag)
        {
			m_edit.restore();
			m_edit.clear();
            viewer->update();
        }
		else
		{
		}
        m_mode = kMode_None;
	}
}

void MaskManipulator::keyReleaseEvent(Viewer *viewer, QKeyEvent *event)
{
    Manipulator::keyReleaseEvent(viewer, event);
}

void MaskManipulator::draw(const Viewer *viewer, QPainter &p) const
{

#if 0
	const Mask *mask = Project::activeProject()->mask();
	QImage image(viewer->getSource()->size(), QImage::Format_Mono);
	std::memset(image.bits(), 0, image.byteCount());
	mask->render(image);
	image = image.convertToFormat(QImage::Format_Indexed8);
	image.save("D:/test.png", "PNG");
	//p.drawImage(0, 0, image);
	return;

	QColor color(255, 0, 0, 64);
	QPen outlinePen(Qt::red, 2.0f);
	outlinePen.setCosmetic(true);
	p.setPen(outlinePen);
	p.setBrush(color);

	QPen handlePen(Qt::red, 1.0f);
	handlePen.setCosmetic(true);

	// draw existing mask shapes
	p.drawPath(mask->path());

	// draw handles
	p.setPen(handlePen);
	p.setBrush(Qt::NoBrush);

	float z = viewer->getZoom();
	float handleSize = viewer->handleSize() / z;

	for (auto shape : mask->shapes())
	{
		const  MaskShape::PointList &points = shape->points();
		for (auto point : points)
		{
			// draw center knot
			const QPointF &pt = point.knot;
			p.drawRect(QRectF(pt.x() - handleSize / 2.0f, pt.y() - handleSize / 2.0f, handleSize, handleSize));

			// draw tangent handles
			if (!point.tan_in.isNull())
			{
				QPointF pt = point.knot + point.tan_in;
				p.drawRect(QRectF(pt.x() - handleSize / 2.0f, pt.y() - handleSize / 2.0f, handleSize, handleSize));
				p.drawLine(point.knot, pt);
			}
			if (!point.tan_out.isNull())
			{
				QPointF pt = point.knot + point.tan_out;
				p.drawRect(QRectF(pt.x() - handleSize / 2.0f, pt.y() - handleSize / 2.0f, handleSize, handleSize));
				p.drawLine(point.knot, pt);
			}
		}
	}

    if (m_rect.isValid())
	{
		p.setPen(outlinePen);
		p.setBrush(color);

		p.drawRect(m_rect);
	}
#endif
}

bool MaskManipulator::canDoCommand(const QString &cmdName) const
{
	return Manipulator::canDoCommand(cmdName);
}

void MaskManipulator::doCommand(const QString &cmdName)
{
	Manipulator::doCommand(cmdName);
}

#endif
