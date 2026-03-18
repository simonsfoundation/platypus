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
	virtual void enterEvent(QEnterEvent *event) override;
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;
	virtual void keyPressEvent(QKeyEvent  *event) override;
	virtual void keyReleaseEvent(QKeyEvent  *event) override;

    virtual void updateView() override;

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
