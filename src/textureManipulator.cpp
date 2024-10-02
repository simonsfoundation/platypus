#include <textureManipulator.h>
#include <removeSource.h>
#include <imageManager.h>
#include <project.h>
#include <polygonCommands.h>
#include <viewer.h>
#include <undoManager.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <cmath>

TextureManipulator::TextureManipulator(QObject *parent) : EditManipulator(parent)
{
}

TextureManipulator::~TextureManipulator()
{
}

Polygon::Type TextureManipulator::polygonType() const
{
    return Polygon::NONE;
}

ImageSource *TextureManipulator::source() const
{
	return ImageManager::get().removeSource();
}

void TextureManipulator::draw(const Viewer *viewer, QPainter &p) const
{
}
