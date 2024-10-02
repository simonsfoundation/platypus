#include <editManipulator.h>
#include <project.h>
#include <clipboard.h>
#include <polygon.h>
#include <viewer.h>
#include <undoManager.h>
#include <polygonCommands.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QClipboard>
#include <QtGui/QBitmap>
#include <QtWidgets/QApplication>
#include <QtCore/QMimeData>
#include <QtCore/QLineF>
#include <cursors/rotate.xbm>
#include <cursors/rotate.xbm.mask>
#include <cursors/crosshair.xpm>
#include <cursors/insert.xbm>
#include <cursors/insert.mask>
#include <cmath>

static QPointF s_downPos;
static QPoint s_clickPos;
static QPoint s_mousePos;
static QPoint s_anchor;
static float s_startAngle;
static QRect s_marquee;

static const int kDragDistance = 2;

static const QCursor rotateCursor()
{
    static const QCursor cursor(
                        QBitmap::fromData(QSize(rotate_width, rotate_height), rotate_bits),
                        QBitmap::fromData(QSize(rotate_width, rotate_height), rotate_mask_bits),
                        rotate_x_hot, rotate_y_hot);

    return cursor;
}

static const QCursor insertCursor()
{
    static const QCursor cursor(
                        QBitmap::fromData(QSize(insert_width, insert_height), insert_bits),
                        QBitmap::fromData(QSize(insert_width, insert_height), insert_mask_bits),
                        insert_x_hot, insert_y_hot);

    return cursor;
}

static const QCursor crosshairCursor()
{
    static const QCursor cursor(QPixmap(crosshairCursor_xpm), 8, 8);

    return cursor;
}

static QRect makeRect(const QPoint &p0, const QPoint &p1)
{
    int left = std::min(int(p0.x()), p1.x());
    int top = std::min(int(p0.y()), p1.y());
    int right = std::max(int(p0.x()), p1.x());
    int bottom = std::max(int(p0.y()), p1.y());

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

EditManipulator::EditManipulator(QObject *parent) : PolygonManipulator(parent)
{
    m_mode = kMode_None;
}

EditManipulator::~EditManipulator()
{
}

Polygon::Type EditManipulator::polygonType() const
{
	return Polygon::INPUT;
}

static bool edgeIsHorizontal(const Polygon::PointList &poly, int index)
{
    int next = index == poly.size() - 1 ? 0 : index + 1;

    const QPoint &p0 = poly[index].knot;
    const QPoint &p1 = poly[next].knot;

    QPoint delta = p1 - p0;
    return abs(delta.x()) > abs(delta.y());
}

QPoint EditManipulator::findAnchor() const
{
	QPoint point(0, 0);
	int count = 0;
	auto selection = this->selection();
	for (auto p : selection)
	{
		for (auto pt : p->points())
		{
			point += pt.knot;
			count++;
		}
	}
	return count == 0 ? point : QPoint(point.x() / count, point.y() / count);
}

void EditManipulator::mouseHoverEvent(Viewer *viewer, QMouseEvent *event)
{
    s_mousePos = event->pos();
    PolygonPointer hoverPolygon;
    int hoverIndex = -1;
    Polygon::Handle hoverHandle = Polygon::kHandle_None;
	QCursor c;
    if (canEdit())
        c = event->modifiers() == Qt::ShiftModifier ? Qt::ArrowCursor : crosshairCursor();
    Polygon::Type type = polygonType();
    m_t = -1.0f;

	// look for selected polygon handles
    const auto &polygons = Project::activeProject()->polygons();
    if (canEdit())
    {
        for (auto polygon : polygons)
        {
            if (polygon->type() == type && polygon->isSelected())
            {
                Polygon::Handle handle;
                int index = findHandle(viewer, *polygon, event->pos(), event->modifiers(), handle);
                if (index >= 0)
                {
                    hoverPolygon = polygon;
                    hoverIndex = index;
                    hoverHandle = handle;
                    c = Qt::ArrowCursor;
                    break;
                }
            }
        }
        if (!hoverPolygon)
        {
            for (auto polygon : polygons)
            {
                if (polygon->type() == type && polygon->isSelected())
                {
                    if (polygon->shapeType() == Polygon::POLYGON)
                    {
                        int index = findEdge(viewer, *polygon, event->pos(), event->modifiers());
                        if (index >= 0)
                        {
                            if (edgeIsHorizontal(polygon->points(), index))
                                c = Qt::SizeVerCursor;
                            else
                                c = Qt::SizeHorCursor;
                            hoverPolygon = polygon;
                        }
                    }
                    else if (event->modifiers() == Qt::AltModifier)
                    {
                        m_t = pointOnShape(polygon, viewer->viewToSource(event->pos()).toPoint(), viewer->handleSize() / viewer->getZoom());
                        if (m_t >= 0.0f)
                        {
                            c = insertCursor();
                            hoverPolygon = polygon;
                        }
                    }
                    if (!hoverPolygon)
                    {
                        if (polygon->path(viewer->getImageSize()).contains(viewer->viewToSource(event->pos()).toPoint()))
                        {
                            c = Qt::SizeAllCursor;
                            hoverPolygon = polygon;
                        }
                    }
                }
                if (hoverPolygon)
                    break;
            }
        }
    }
    else
    {
        for (auto polygon : polygons)
        {
            if (polygon->type() == type && polygon->isSelected())
            {
                if (polygon->path(viewer->getImageSize()).contains(viewer->viewToSource(event->pos()).toPoint()))
                {
                    c = Qt::PointingHandCursor;
                    hoverPolygon = polygon;
                }
            }
        }
    }

    // check if over a non-selected polygon, and show selection cursor
    if (!hoverPolygon)
    {
        for (auto polygon : polygons)
        {
            if (polygon->type() == type && !polygon->isSelected())
            {
                if (polygon->path(viewer->getImageSize()).contains(viewer->viewToSource(event->pos()).toPoint()))
                {
					c = Qt::PointingHandCursor;
                }
            }
        }
    }
    if (canEdit())
    {
        if (!hoverPolygon && event->modifiers() == Qt::AltModifier)
            c = rotateCursor();
    }
	viewer->setCursor(c);
    if (setHover(hoverPolygon, hoverIndex, hoverHandle))
        viewer->update();

	if (!event->isAccepted())
	    Manipulator::mouseHoverEvent(viewer, event);
}

void EditManipulator::mousePressEvent(Viewer *viewer, QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
	{
		s_downPos = viewer->viewToSource(event->pos());
		s_clickPos = event->pos();

		// shortcut for creating a new polygon
        if (canEdit())
        {
            if (event->modifiers() == (Qt::AltModifier | Qt::ShiftModifier))
            {
                createPolygon(viewer, event);
                return;
            }
        }

		// look for an existing polygon
		PolygonPointer clickedPolygon = hoverPolygon();
        if (!clickedPolygon)
        {
            auto polygons = Project::activeProject()->polygons(polygonType());
            for (auto polygon : polygons)
            {
                if (polygon->path(viewer->getImageSize()).contains(viewer->viewToSource(event->pos()).toPoint()))
                    clickedPolygon = polygon;
            }
        }
		if (clickedPolygon)
		{
            PolygonList selection = this->selection();
			if (event->modifiers() & Qt::ControlModifier)
				UndoManager::instance()->push(new SelectPolygonsCommand(clickedPolygon, !clickedPolygon->isSelected()));
            else if (!clickedPolygon->isSelected())
				UndoManager::instance()->push(new SelectPolygonsCommand(clickedPolygon));

			event->accept();

            if (m_t >= 0.0f)
            {
                Edit edit(clickedPolygon);
                int index = clickedPolygon->insertPoint(m_t);
                edit.selection << index;
                edit.handle = Polygon::kHandle_Knot;
                edit.point = clickedPolygon->point(index);
                m_edits << edit;
                m_mode = kMode_Insert;
            }
		}
		else if (event->modifiers() == 0)
        {
            // deselect all
            if (!selection().empty())
                UndoManager::instance()->push(new SelectPolygonsCommand(selection(), false));
            createPolygon(viewer, event);
        }
        else if (event->modifiers() == Qt::ShiftModifier)
            beginSelect(viewer, event);
		else if (event->modifiers() == Qt::AltModifier)
		{
			event->accept();
		}
	}
	if (!event->isAccepted())
		Manipulator::mousePressEvent(viewer, event);
}

void EditManipulator::createPolygon(Viewer *viewer, QMouseEvent *event)
{
    if (canEdit())
    {
        s_downPos = viewer->viewToSource(event->pos());
        s_clickPos = event->pos();

        if (polygonType() != Polygon::NONE)
        {
            // create a new one
            m_polygon = PolygonPointer(new Polygon(polygonType()));
            m_mode = kMode_Draw;

            QPolygon poly;
            poly << s_downPos.toPoint();
            m_polygon->set(poly);
            viewer->update();
            event->accept();
        }
    }
}

static float calcAngle(const Viewer *viewer, const QPoint &mouse)
{
	QPoint p = (viewer->viewToSource(mouse) - s_anchor).toPoint();
	QSize size = viewer->getSource()->size();
	QPoint delta(size.width() / 2, size.height() / 2);
	float angle = std::atan2(p.y(), p.x()) - std::atan2(delta.y(), delta.x());
	return angle;
}

void EditManipulator::mouseMoveEvent(Viewer *viewer, QMouseEvent *event)
{
    s_mousePos = event->pos();

	if (m_mode == kMode_None && canEdit())
	{
		if ((event->pos() - s_clickPos).manhattanLength() > kDragDistance)
		{
            PolygonPointer clicked = hoverPolygon();
			if (clicked)
			{
				// are we over a handle?
                if (hoverHandle() != Polygon::kHandle_None)
                {
                    Edit edit(clicked);
                    edit.selection << hoverIndex();
                    edit.handle = hoverHandle();
                    m_edits << edit;
                }
                else
                {
                    int index = findEdge(viewer, *clicked, s_clickPos, event->modifiers());
                    if (index >= 0)
                    {
                        Edit edit(clicked);
                        edit.selection << index;
                        if (index == clicked->points().size() - 1)
                            edit.selection << 0;
                        else
                            edit.selection << index + 1;
                        m_edits << edit;
                    }
                }
				if (m_edits.empty() && clicked)
				{
					QList<PolygonPointer> selection = Project::activeProject()->selection(polygonType());
					for (auto p : selection)
						m_edits << Edit(p);
				}
				if (!m_edits.empty())
				{
					m_mode = kMode_Drag;
				}
			}
			else if (event->modifiers() == Qt::AltModifier)
			{
				QList<PolygonPointer> selection = Project::activeProject()->selection(polygonType());
				for (auto p : selection)
					m_edits << Edit(p);
				m_mode = kMode_Rotate;
				s_anchor = findAnchor();
				s_startAngle = calcAngle(viewer, event->pos());
			}
		}
	}

    switch (m_mode)
    {
        case kMode_Select:
        {
            doSelect(viewer, event);
            break;
        }
        case kMode_Draw:
		{
            if (m_polygon)
            {
                QPolygon poly;
                QPoint pos = viewer->viewToSource(event->pos()).toPoint();
                poly << s_downPos.toPoint();
                poly << QPoint(pos.x(), s_downPos.y());
                poly << pos;
                poly << QPoint(s_downPos.x(), pos.y());
                m_polygon->set(poly);
                viewer->update();
                event->accept();
            }
            break;
		}
		case kMode_Drag:
		{
			QPoint delta = (viewer->viewToSource(event->pos()) - s_downPos).toPoint();
			for (auto edit : m_edits)
				edit.poly->set(edit.move(delta, event->modifiers()));
			break;
		}
		case kMode_Insert:
		{
			QPoint delta = (viewer->viewToSource(event->pos()) - s_downPos).toPoint();
            Q_ASSERT(m_edits.size() == 1);
			for (auto edit : m_edits)
            {
                Q_ASSERT(edit.selection.size() == 1);
                Polygon::Point p = edit.point;
                p.knot += delta;
				edit.poly->setPoint(edit.selection[0], p);
            }
			break;
		}
		case kMode_Rotate:
		{
			float angle = calcAngle(viewer, event->pos());
			angle = angle - s_startAngle;

			for (auto edit : m_edits)
				edit.poly->set(edit.rotate(s_anchor, angle));
			break;
		}
    }
	if (!event->isAccepted())
		Manipulator::mouseMoveEvent(viewer, event);
}

void EditManipulator::mouseReleaseEvent(Viewer *viewer, QMouseEvent *event)
{
    switch (m_mode)
    {
		case kMode_None:
		{
			// if nothing happened and we clicked on a polygon, replace the selection
			if (event->modifiers() == 0 && 0)
			{
				PolygonPointer polygon = hoverPolygon();
				if (polygon)
					UndoManager::instance()->push(new SelectPolygonsCommand(polygon));
			}
			break;
		}
        case kMode_Select:
        {
            endSelect(viewer, event);
            break;
        }
        case kMode_Draw:
		{
            if (m_polygon)
            {
				if (m_polygon->points().size() == 4)
				{
					UndoManager::instance()->beginMacro(tr("Add Polygon"));
					UndoManager::instance()->push(new AddPolygonCommand(m_polygon));
					UndoManager::instance()->push(new SelectPolygonsCommand(m_polygon));
					UndoManager::instance()->endMacro();
				}
                m_polygon.reset();
				event->accept();
            }
            break;
		}

		case kMode_Drag:
		{
			UndoManager::instance()->beginMacro(tr("Edit Polygons"));
			QPoint delta = (viewer->viewToSource(event->pos()) - s_downPos).toPoint();

			for (auto edit : m_edits)
			{
				edit.poly->set(edit.backup);
				UndoManager::instance()->push(new EditPolygonCommand(edit.poly, edit.move(delta, event->modifiers())));
			}
			UndoManager::instance()->endMacro();
			m_edits.clear();
			event->accept();
			break;
		}

		case kMode_Insert:
		{
			UndoManager::instance()->beginMacro(tr("Insert Point"));
            Q_ASSERT(m_edits.size() == 1);

			for (auto edit : m_edits)
			{
                Polygon::PointList points = edit.poly->points();
				edit.poly->set(edit.backup);
				UndoManager::instance()->push(new EditPolygonCommand(edit.poly, points));
			}
			UndoManager::instance()->endMacro();
			m_edits.clear();
			event->accept();
			break;
		}

		case kMode_Rotate:
		{
			UndoManager::instance()->beginMacro(tr("Rotate Polygons"));
			float angle = calcAngle(viewer, event->pos());
			angle = angle - s_startAngle;

			for (auto edit : m_edits)
			{
				edit.poly->set(edit.backup);
				UndoManager::instance()->push(new EditPolygonCommand(edit.poly, edit.rotate(s_anchor, angle)));
			}
			UndoManager::instance()->endMacro();
			m_edits.clear();
			event->accept();
			break;
		}
    }
	m_mode = kMode_None;
	if (!event->isAccepted())
		Manipulator::mouseReleaseEvent(viewer, event);

    mouseHoverEvent(viewer, event);
}

void EditManipulator::keyPressEvent(Viewer *viewer, QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_Shift)
    {
        QMouseEvent mouseEvent(QEvent::MouseMove, s_mousePos, Qt::NoButton, 0, Qt::AltModifier);
        mouseHoverEvent(viewer, &mouseEvent);
    }
	if (!event->isAccepted())
		Manipulator::keyPressEvent(viewer, event);
}

void EditManipulator::keyReleaseEvent(Viewer *viewer, QKeyEvent *event)
{
    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_Shift)
    {
        QMouseEvent mouseEvent(QEvent::MouseMove, s_mousePos, Qt::NoButton, 0, 0);
        mouseHoverEvent(viewer, &mouseEvent);
    }
    Manipulator::keyReleaseEvent(viewer, event);
}

void EditManipulator::cancel(Viewer *viewer)
{
	if (m_mode != kMode_None)
	{
        if (m_mode == kMode_Draw)
        {
            m_polygon.reset();
            viewer->update();
        }
		else
		{
			for (auto edit : m_edits)
				edit.poly->set(edit.backup);
			m_edits.clear();
		}
        m_mode = kMode_None;
	}
}

void EditManipulator::draw(const Viewer *viewer, QPainter &p) const
{
	PolygonManipulator::draw(viewer, p);

    if (m_polygon)
	{
        drawPolygon(viewer, p, *m_polygon);
	}

    if (s_marquee.isValid())
    {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 0.0f, Qt::DashLine));
        p.drawRect(s_marquee);
    }
}

//
// Edit
//

static float angle(const QPointF &p0, const QPointF &p1)
{
    return std::atan2(p1.y() - p0.y(), p1.x() - p0.x());
}

Polygon::PointList EditManipulator::Edit::move(const QPoint &delta, int modifiers) const
{
	Polygon::PointList poly = backup;
	if (selection.isEmpty())
    {
		for (int i = 0; i < poly.size(); i++)
			poly[i].knot += delta;
    }
	else
	{
		for (int i : selection)
        {
            Polygon::Point &p = poly[i];

            if (handle == Polygon::kHandle_Knot || handle == Polygon::kHandle_None)
                p.knot += delta;
            else if (handle == Polygon::kHandle_InTangent || handle == Polygon::kHandle_OutTangent)
            {
                QPointF this_tan = handle == Polygon::kHandle_InTangent ? p.knot + p.tan_in : p.knot + p.tan_out;
                QPointF that_tan = handle == Polygon::kHandle_InTangent ? p.knot + p.tan_out : p.knot + p.tan_in;
                float this_dist = QLineF(this_tan, p.knot).length();
                float that_dist = QLineF(that_tan, p.knot).length();
                if (this_dist > 0.0f && that_dist > 0.0f)
                {
                    float this_angle = angle(p.knot, this_tan);
                    float that_angle = angle(p.knot, that_tan);
                    QPointF new_tan = this_tan + delta;
                    if (modifiers & Qt::ControlModifier)
                        that_dist = QLineF(p.knot, new_tan).length();

                    float a = that_angle + (angle(p.knot, new_tan) - this_angle);

                    that_tan.setX(p.knot.x() + (that_dist * std::cos(a)));
                    that_tan.setY(p.knot.y() + (that_dist * std::sin(a)));

                    if (handle == Polygon::kHandle_InTangent)
                    {
                        p.tan_in = new_tan.toPoint() - p.knot;
                        p.tan_out = that_tan.toPoint() - p.knot;
                    }
                    else
                    {
                        p.tan_out = new_tan.toPoint() - p.knot;
                        p.tan_in = that_tan.toPoint() - p.knot;
                    }
                }
            }
        }
	}
	return poly;
}

Polygon::PointList EditManipulator::Edit::rotate(const QPoint &anchor, float angle) const
{
	QTransform translate = QTransform::fromTranslate(anchor.x(), anchor.y());
	QTransform rotation;
	rotation.rotate(angle * 180.0f / 3.1415926535f);
	QTransform xform = translate.inverted() * rotation * translate;
    Polygon::PointList result;
    for (Polygon::Point p : backup)
    {
        QPoint knot = p.knot;
        p.knot = p.knot * xform;
        p.tan_in = (p.tan_in + knot) * xform - p.knot;
        p.tan_out = (p.tan_out + knot) * xform - p.knot;
        result << p;
    }
	return result;
}

//
// COMMANDS
//

bool EditManipulator::canDoCommand(const QString &cmdName) const
{
	if (cmdName == "cut" || cmdName == "copy" || cmdName == "delete" || cmdName == "invertMask")
		return Project::activeProject()->hasSelection(polygonType());
	else if (cmdName == "paste")
		return Clipboard::instance()->hasType(&Polygon::staticMetaObject);
	else if (cmdName == "selectAll")
		return !Project::activeProject()->polygons(polygonType()).empty();
	return Manipulator::canDoCommand(cmdName);
}

void EditManipulator::doCommand(const QString &cmdName)
{
	if (cmdName == "cut")
	{
		UndoManager::instance()->beginMacro(tr("Cut"));
		doCommand("copy");
		doCommand("delete");
		UndoManager::instance()->endMacro();
	}
	else if (cmdName == "copy")
	{
		QObjectList objects;
		for (auto p : selection())
			objects << p->clone();
		Clipboard::instance()->set(objects);
	}
	else if (cmdName == "paste")
	{
		UndoManager::instance()->beginMacro(tr("Paste"));

		// deselect all first
		UndoManager::instance()->push(new SelectPolygonsCommand(selection(), false));
		const QObjectList &clipboard = Clipboard::instance()->objects();
		QList<PolygonPointer> newSelection;
		for (auto object : clipboard)
		{
			Polygon *poly = qobject_cast<Polygon *>(object);
			if (poly)
			{
				PolygonPointer copy(poly->clone());
                copy->translate(QPoint(100, 100));
				newSelection << copy;
				UndoManager::instance()->push(new AddPolygonCommand(copy));
			}
		}
		UndoManager::instance()->push(new SelectPolygonsCommand(newSelection, true));

		UndoManager::instance()->endMacro();
	}
	else if (cmdName == "delete")
	{
        if (hoverPolygon() && hoverIndex() >= 0 && hoverPolygon()->shapeType() == Polygon::BEZIER)
        {
            PolygonPointer p = hoverPolygon();
            if (p->points().size() > 3)
            {
                Polygon::PointList old_points = p->points();
                p->deletePoint(hoverIndex());
                Polygon::PointList new_points = p->points();
                p->set(old_points);
                UndoManager::instance()->push(new EditPolygonCommand(p, new_points));
            }
        }
        else
        {
            UndoManager::instance()->beginMacro(tr("Delete"));
            for (auto p : selection())
                UndoManager::instance()->push(new RemovePolygonCommand(p));
            UndoManager::instance()->endMacro();
        }
	}
	else if (cmdName == "selectAll")
	{
		UndoManager::instance()->push(new SelectPolygonsCommand(Project::activeProject()->polygons(polygonType()), true));
	}
    else if (cmdName == "invertMask")
    {
        UndoManager::instance()->beginMacro(tr("Invert Mask"));
        for (auto p : selection())
            UndoManager::instance()->push(new EditPolygonCommand(p, "invert", !p->value("invert").toBool()));
        UndoManager::instance()->endMacro();
   }
}

void EditManipulator::nudge(Viewer *viewer, const QPointF &delta)
{
    if (canEdit())
    {
        QPoint shift = delta.toPoint();
        UndoManager::instance()->beginMacro(tr("Nudge"));
        auto selection = this->selection();
        int index = hoverIndex();
        for (auto p : selection)
        {
            Polygon::PointList points = p->points();
            if (p == hoverPolygon() && index >= 0)
            {
                points[index].knot += shift;
            }
            else
            {
                for (Polygon::Point &pt : points)
                    pt.knot += shift;
            }
            UndoManager::instance()->push(new EditPolygonCommand(p, points));
        }
        UndoManager::instance()->endMacro();
    }
}

static PolygonList s_selection;

void EditManipulator::beginSelect(Viewer *viewer, QMouseEvent *event)
{
    m_mode = kMode_Select;
    QPoint pos = viewer->viewToSource(event->pos()).toPoint();
    s_marquee = QRect(pos, QSize(1, 1));
    viewer->update();
    event->accept();

    // save the current selection
    s_selection = selection();
}

void EditManipulator::doSelect(Viewer *viewer, QMouseEvent *event)
{
    QPoint pos = viewer->viewToSource(event->pos()).toPoint();

    s_marquee = makeRect(s_downPos.toPoint(), pos);

    auto &polygons = Project::activeProject()->polygons();
    for (auto p : polygons)
        p->setSelected(p->path(viewer->getImageSize()).intersects(s_marquee));

    viewer->update();
    event->accept();
}

void EditManipulator::endSelect(Viewer *viewer, QMouseEvent *event)
{
    // restore the selection
    PolygonList selection;
    auto &polygons = Project::activeProject()->polygons();
    for (auto p : polygons)
    {
        if (p->isSelected())
            selection << p;
        p->setSelected(s_selection.contains(p));
    }
    s_selection.clear();

    UndoManager::instance()->push(new SelectPolygonsCommand(selection));

    s_marquee = QRect();
    Q_ASSERT(!s_marquee.isValid());
    viewer->update();
    event->accept();
}

bool EditManipulator::canEdit() const
{
    return true;
}
