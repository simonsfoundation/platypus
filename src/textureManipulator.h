#ifndef TEXTUREMANIPULATOR_H
#define TEXTUREMANIPULATOR_H

#include <editManipulator.h>
#include <QtCore/QRect>

class TextureManipulator : public EditManipulator
{
	Q_OBJECT
public:
	TextureManipulator(QObject *parent = nullptr);
	~TextureManipulator();

	virtual Polygon::Type polygonType() const;

	virtual ImageSource *source() const;

    virtual void draw(const Viewer *viewer, QPainter &p) const;

protected:

private:
};

#endif
