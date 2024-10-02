#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <project.h>
#include <QtWidgets/QMainWindow>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(Project *project, QWidget *parent = 0);
	~MainWindow();

	void open(const QString &path, bool pluginMode = false);
	void loadCradle(const QString &path);

	enum Tab
	{
		kTab_Mark,
		kTab_Remove,
		kTab_Texture
	};

    void progress(int value, int total) const;

    bool canceled() const;

protected:
	void setupMenus();
	void save(const QString &path);
	bool saveAs();
	bool doExport();

	virtual bool eventFilter(QObject *object, QEvent *event);
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void onOpen();
	void onClose();
	void onSave();
	void onSaveAs();
	void onOpenImage();
    void onExport();
	void onExit();
	void onImageChanged();
	void updateMenus();
	void updateUndo();
    void updateSliders();
	void onTool();
	void onToggleOverlay();
	void onToggleResult();
	void onUndoRedoChanged(bool);
	void onUndoRedoText(const QString &);

	void onCommand();
    void onDelete();

	void onTab(int);
	void onSelection();
	void onToolChanged();

    void onDetectCradle();
    void onRemoveCradle();
    void onRemoveTexture();

    void beginProgress(const QString &msg);
    void endProgress();

    void onCancelProgress();
    void onReset();

    void onDone();
    void onCancel();

    void onPolygonAdded(Polygon *poly);
    void onPolygonRemoved(Polygon *poly);
    void onPolygonValueChanged(Polygon *poly, const QString &);

    void onSlider(int);
    void onBeginChange();
    void onEndChange();

    void onHelp();
    void onAbout();

private:
	Project *m_project;
	class Viewer *m_viewer;
	class ViewerScroller *m_scroller;
	class UndoManager *m_undoMgr;
	class Clipboard *m_clipboard;
	class QTabBar *m_tabs;
    class QProgressBar *m_progressBar;
    class QPushButton *m_cancel;

	// menus
	class QMenu *m_editMenu;
	class QMenu *m_viewMenu;
    class QAction *m_loadCradle;
    class QAction *m_saveCradle;
	class QAction *m_close;
	class QAction *m_save;
	class QAction *m_saveAs;
	class QAction *m_open;
	class QAction *m_export;
	class QAction *m_undo;
	class QAction *m_redo;
	class QAction *m_cut;
	class QAction *m_copy;
	class QAction *m_paste;
	class QAction *m_delete;
	class QAction *m_selectAll;
	QList<QAction *> m_commands;
    class QAction *m_detectCradle;
    class QAction *m_removeCradle;
    class QAction *m_removeTexture;
	class QAction *m_showResult;
    class Slider *m_black;
    class Slider *m_gamma;
    class Slider *m_white;

	class QAction *m_doneAction;
	class QAction *m_cancelAction;

    class QAction *m_reset;

	// tools
	class QToolBar *m_viewerTools;
	class QToolBar *m_tools;
	class QAction *m_polygon;
	class QAction *m_mask;
	class QAction *m_overlay;
    class QAction *m_invert;
	QList<QAction *> m_toolActions;

	bool m_pluginMode;
};

#endif // MAINWINDOW_H
