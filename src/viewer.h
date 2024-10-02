#ifndef VIEWER_H
#define VIEWER_H

#include <imageViewer.h>

class Manipulator;

class Viewer : public ImageViewer
{
	Q_OBJECT

public:
	Viewer(QWidget *parent = nullptr);
	~Viewer();

	enum Tool
	{
		kTool_None,
		kTool_Polygon,
		kTool_Mask,
		kTool_Remove,
		kTool_Texture
	};

	bool overlayEnabled() const { return m_overlay; }
	Tool tool() const { return m_tool; }
	float handleSize() const;

    const QTransform &viewTransform() const;
	Manipulator *manipulator() const;

	bool hitPoint(const QPointF &pos, const QPointF &mouse) const;

Q_SIGNALS:
	void toolChanged();

public slots:
	void enableOverlay(bool state);
	void setTool(Tool tool);

protected:
	virtual void enterEvent(QEvent *event);
	virtual void paintEvent(QPaintEvent *event);
	virtual void resizeEvent(QResizeEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void keyPressEvent(QKeyEvent  *event);
	virtual void keyReleaseEvent(QKeyEvent  *event);

    virtual void updateView();

    bool manipulatorShouldReceiveEvents() const;

private slots:
	void onImageChanged();
	void updateZoomRange();
    void setManipulator(Manipulator *m);

private:
	enum Action
	{
		kAction_None,
        kAction_Manipulator
	};
    QTransform m_xform;
    Manipulator *m_manipulator;
	Action m_action;
	Tool m_tool;
	float m_handleSize;
	bool m_overlay;
};

inline const QTransform &Viewer::viewTransform() const { return m_xform; }
inline float Viewer::handleSize() const { return m_handleSize; }
inline Manipulator *Viewer::manipulator() const { return m_manipulator; }


#endif
