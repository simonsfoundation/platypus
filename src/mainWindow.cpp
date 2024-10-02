#include <mainWindow.h>
#include <imageManager.h>
#include <viewer.h>
#include <clipboard.h>
#include <manipulator.h>
#include <viewerScroller.h>
#include <project.h>
#include <polygon.h>
#include <undoManager.h>
#include <polygonCommands.h>
#include <removeSource.h>
#include <detectCradleDialog.h>
#include <slider.h>
#include <platypus/CradleFunctions.h>
#include <platypus/TextureRemoval.h>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QShortcut>
#include <QtWidgets/QBoxLayout>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QDesktopServices>
#include <QtCore/QSettings>
#include <QtCore/QLine>

static const char *kProjectExtension = "platypus";

MainWindow::MainWindow(Project *project, QWidget *parent) : QMainWindow(parent),
	m_project(project),
	m_undoMgr(new UndoManager(this)),
	m_clipboard(new Clipboard(this)),
	m_pluginMode(false)
{
	connect(&ImageManager::get(), SIGNAL(imageChanged()), SLOT(onImageChanged()));
	connect(&ImageManager::get(), SIGNAL(status(const QString &)), statusBar(), SLOT(showMessage(const QString &)));
	connect(m_undoMgr, SIGNAL(canRedoChanged(bool)), SLOT(onUndoRedoChanged(bool)));
	connect(m_undoMgr, SIGNAL(canUndoChanged(bool)), SLOT(onUndoRedoChanged(bool)));
	connect(m_undoMgr, SIGNAL(redoTextChanged(const QString &)), SLOT(onUndoRedoText(const QString &)));
	connect(m_undoMgr, SIGNAL(undoTextChanged(const QString &)), SLOT(onUndoRedoText(const QString &)));
	connect(m_clipboard, SIGNAL(changed()), SLOT(updateMenus()));

	connect(Project::activeProject(), SIGNAL(selectionChanged()), SLOT(onSelection()));
	connect(Project::activeProject(), SIGNAL(polygonAdded(Polygon *)), SLOT(onPolygonAdded(Polygon *)));
	connect(Project::activeProject(), SIGNAL(polygonRemoved(Polygon *)), SLOT(onPolygonRemoved(Polygon *)));
	connect(Project::activeProject(), SIGNAL(polygonValueChanged(Polygon *, const QString &)), SLOT(onPolygonValueChanged(Polygon *, const QString &)));

	m_viewer = new Viewer;

	connect(m_viewer, SIGNAL(toolChanged()), SLOT(onToolChanged()));

	m_scroller = new ViewerScroller(m_viewer);

    // VIEWER TOOLS
    m_viewerTools = new QToolBar;
    m_viewerTools->setObjectName("viewerTools");
    m_viewerTools->setIconSize(QSize(24, 24));
    {
        m_doneAction = m_viewerTools->addAction(QIcon(":images/done.png"), tr("Done"), this, SLOT(onDone()));
        m_cancelAction = m_viewerTools->addAction(QIcon(":images/cancel.png"), tr("Cancel"), this, SLOT(onCancel()));

        m_viewerTools->addSeparator();

        QAction *reset = m_viewerTools->addAction(QIcon(":images/reset.png"), tr("Reset"), this, SLOT(onReset()));
        reset->setToolTip(tr("Reset All"));

        m_viewerTools->addSeparator();

        QAction *zoom100 = m_viewerTools->addAction(QIcon(":images/1to1.png"), tr("Zoom 100%"), m_viewer, SLOT(zoom100()));
        zoom100->setToolTip(tr("Zoom 1:1 (h)"));

        QAction *zoomFit = m_viewerTools->addAction(QIcon(":images/fitImage.png"), tr("Zoom to Fit"), m_viewer, SLOT(zoomToFit()));
        zoomFit->setToolTip(tr("Zoom to Fit (f)"));

        QAction *zoomIn = m_viewerTools->addAction(QIcon(":images/zoomIn.png"), tr("Zoom In"), m_viewer, SLOT(zoomIn()));
        zoomIn->setToolTip(tr("Zoom In (i)"));

        QAction *zoomOut = m_viewerTools->addAction(QIcon(":images/zoomOut.png"), tr("Zoom Out"), m_viewer, SLOT(zoomOut()));
        zoomOut->setToolTip(tr("Zoom Out (o)"));

        #if defined(Q_OS_MACX)
            zoom100->setShortcut(QKeySequence("h"));
            zoomFit->setShortcut(QKeySequence("f"));
            zoomIn->setShortcut(QKeySequence("i"));
            zoomOut->setShortcut(QKeySequence("o"));
        #endif

        m_overlay = m_viewerTools->addAction(QIcon(":images/overlay.png"), tr("Show Overlay"), this, SLOT(onToggleOverlay()));
        m_overlay->setToolTip(tr("Toggle Overlay (q)"));
        m_overlay->setCheckable(true);
        m_overlay->setChecked(m_viewer->overlayEnabled());
        m_overlay->setShortcut(QKeySequence("q"));
    }

	// TOOLS
	m_tools = new QToolBar;
    m_tools->setObjectName("tools");
    m_tools->setIconSize(QSize(24, 24));

	// EDIT
	m_polygon = m_tools->addAction(QIcon(":images/polygon.png"), tr("Polygons"), this, SLOT(onTool()));
	m_polygon->setToolTip(tr("Polygons (e)"));
	m_polygon->setShortcut(QKeySequence("e"));
	m_polygon->setCheckable(true);
	m_polygon->setData(Viewer::kTool_Polygon);
    m_polygon->setStatusTip(tr("Edit cradle polygons"));

	// MASK
	m_mask = m_tools->addAction(QIcon(":images/mask.png"), tr("Mask"), this, SLOT(onTool()));
	m_mask->setToolTip(tr("Mask (m)"));
	m_mask->setShortcut(QKeySequence("m"));
	m_mask->setCheckable(true);
	m_mask->setData(Viewer::kTool_Mask);
    m_mask->setStatusTip(tr("Edit defect mask"));

	m_toolActions << m_polygon;
	m_toolActions << m_mask;

	m_tools->addSeparator();

	// INVERT
	m_invert = m_tools->addAction(QIcon(":images/invert.png"), tr("Invert"), this, SLOT(onCommand()));
	m_invert->setToolTip(tr("Invert Mask (Ctrl+i)"));
	m_invert->setShortcut(QKeySequence("Ctrl+i"));
	m_invert->setData("invertMask");
    m_invert->setStatusTip(tr("Invert mask shape"));
    m_commands << m_invert;

    // DETECT CRADLE
	m_detectCradle = m_tools->addAction(QIcon(":images/detectCradle.png"), tr("Detect Cradle"), this, SLOT(onDetectCradle()));
    m_detectCradle->setStatusTip(tr("Detect cradle"));

	m_removeCradle = m_tools->addAction(QIcon(":images/removeCradle.png"), tr("Remove Cradle"), this, SLOT(onRemoveCradle()));
    m_removeCradle->setStatusTip(tr("Remove cradle"));

	m_removeTexture = m_tools->addAction(tr("Remove Texture"), this, SLOT(onRemoveTexture()));
    m_removeTexture->setStatusTip(tr("Remove texture"));

	// REMOVAL TOOLS
	m_showResult = m_tools->addAction(tr("Show Result"), this, SLOT(onToggleResult()));
	m_showResult->setToolTip(tr("Toggle Result (r)"));
	m_showResult->setCheckable(true);
	m_showResult->setChecked(true);
	m_showResult->setShortcut(QKeySequence("r"));
    m_showResult->setStatusTip(tr("Toggle result image display"));

    // INTENSITY
    {
        QWidget *spacer = new QWidget;
        spacer->setFixedWidth(16);
        m_tools->addWidget(spacer);

		QWidget *sliders = new QWidget;
		QHBoxLayout *layout = new QHBoxLayout(sliders);
		layout->setMargin(0);

        m_black = new Slider;
        m_black->setObjectName("black");
        m_black->setLabel(tr("Black"));
        m_black->setRange(0, 255);
        connect(m_black, SIGNAL(sliderPressed()), SLOT(onBeginChange()));
        connect(m_black, SIGNAL(sliderReleased()), SLOT(onEndChange()));
        connect(m_black, SIGNAL(valueChanged(int)), SLOT(onSlider(int)));

        m_gamma = new Slider;
        m_gamma->setObjectName("gamma");
        m_gamma->setLabel(tr("Gamma"));
        m_gamma->setRange(-100, 100);
        connect(m_gamma, SIGNAL(sliderPressed()), SLOT(onBeginChange()));
        connect(m_gamma, SIGNAL(sliderReleased()), SLOT(onEndChange()));
        connect(m_gamma, SIGNAL(valueChanged(int)), SLOT(onSlider(int)));

        m_white = new Slider;
        m_white->setObjectName("white");
        m_white->setLabel(tr("White"));
        m_white->setRange(0, 255);
        connect(m_white, SIGNAL(sliderPressed()), SLOT(onBeginChange()));
        connect(m_white, SIGNAL(sliderReleased()), SLOT(onEndChange()));
        connect(m_white, SIGNAL(valueChanged(int)), SLOT(onSlider(int)));

		layout->addWidget(m_black);
		layout->addWidget(m_gamma);
		layout->addWidget(m_white);
		layout->addStretch(100);
        m_tools->addWidget(sliders);
    }

	addToolBar(Qt::TopToolBarArea, m_viewerTools);
	addToolBar(Qt::TopToolBarArea, m_tools);

	// TABS
	m_tabs = new QTabBar;
	connect(m_tabs, SIGNAL(currentChanged(int)), SLOT(onTab(int)));
	m_tabs->blockSignals(true);
	m_tabs->setExpanding(false);
	m_tabs->addTab(tr("Mark"));
	m_tabs->addTab(tr("Remove"));
	m_tabs->addTab(tr("Texture"));
	m_tabs->blockSignals(false);

	QWidget *widget = new QWidget;
	QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(0);
    layout->setSpacing(0);
	layout->addWidget(m_tabs);
	layout->addWidget(m_scroller);
	setCentralWidget(widget);

	m_tools->setEnabled(false);
    m_viewerTools->setEnabled(false);
    m_tabs->setEnabled(false);

    m_progressBar = new QProgressBar;
    m_cancel = new QPushButton(tr("Cancel"));
    m_cancel->setFixedHeight(20);
    connect(m_cancel, SIGNAL(clicked()), SLOT(onCancelProgress()));
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_cancel);
    m_progressBar->hide();
    m_cancel->hide();
	statusBar()->show();

	setupMenus();
    setWindowTitle(QCoreApplication::applicationName());

    // set draw tool
    m_viewer->setTool(Viewer::kTool_Polygon);
}

MainWindow::~MainWindow()
{

}

void MainWindow::setupMenus()
{
	// file menu
	{
		QMenu *menu = menuBar()->addMenu(tr("&File"));

		// Open
		m_open = menu->addAction(tr("&Open Image...", "File|Open"));
		connect(m_open, SIGNAL(triggered()), this, SLOT(onOpenImage()));

		// Export
		m_export = menu->addAction(tr("&Export...", "File|Export"));
		connect(m_export, SIGNAL(triggered()), this, SLOT(onExport()));

		// Close
		m_close = menu->addAction(tr("&Close", "File|Close"));
		m_close->setShortcut(QKeySequence::Close);
		connect(m_close, SIGNAL(triggered()), this, SLOT(onClose()));

        // open cradle
		m_loadCradle = menu->addAction(tr("Open &Cradle...", "File|Open"));
		m_loadCradle->setShortcut(QKeySequence::Open);
		connect(m_loadCradle, SIGNAL(triggered()), this, SLOT(onOpen()));

		// Save
		m_save = menu->addAction(tr("&Save Cradle", "File|Save"));
		m_save->setShortcut(QKeySequence::Save);
		connect(m_save, SIGNAL(triggered()), this, SLOT(onSave()));

		// Save As
		m_saveAs = menu->addAction(tr("&Save Cradle As...", "File|Save As"));
		m_saveAs->setShortcut(QKeySequence::SaveAs);
		connect(m_saveAs, SIGNAL(triggered()), this, SLOT(onSaveAs()));

		menu->addSeparator();

		// Exit
		QAction *exit = menu->addAction(tr("E&xit"));
		exit->setShortcut(tr("Ctrl+Q", "File|Exit"));
		connect(exit, SIGNAL(triggered()), this, SLOT(onExit()));
	}

	// edit menu
	{
		m_editMenu = menuBar()->addMenu(tr("&Edit"));

		m_undo = m_editMenu->addAction(tr("&Undo", "Edit|Undo"));
		m_undo->setShortcut(QKeySequence::Undo);
		connect(m_undo, SIGNAL(triggered()), m_undoMgr, SLOT(undo()));

		m_redo = m_editMenu->addAction(tr("&Redo", "Edit|Redo"));
		m_redo->setShortcut(QKeySequence::Redo);
		connect(m_redo, SIGNAL(triggered()), m_undoMgr, SLOT(redo()));

		m_editMenu->addSeparator();

		// Cut
		{
			QAction *action = m_editMenu->addAction(tr("Cu&t", "Edit|Cut"));
			action->setShortcut(QKeySequence::Cut);
			action->setData("cut");
			connect(action, SIGNAL(triggered()), this, SLOT(onCommand()));
			m_commands << action;
		}

		// Copy
		{
			QAction *action = m_editMenu->addAction(tr("&Copy", "Edit|Copy"));
			action->setShortcut(QKeySequence::Copy);
			action->setData("copy");
			connect(action, SIGNAL(triggered()), this, SLOT(onCommand()));
			m_commands << action;
            m_copy = action;
		}

		// Paste
		{
			QAction *action = m_editMenu->addAction(tr("&Paste", "Edit|Paste"));
			action->setShortcut(QKeySequence::Paste);
			action->setData("paste");
			connect(action, SIGNAL(triggered()), this, SLOT(onCommand()));
			m_commands << action;
            m_paste = action;
		}

		// Delete
		{
			QAction *action = m_editMenu->addAction(tr("&Delete", "Edit|Delete"));
            #if defined(Q_OS_MACX)
                action->setShortcut(QKeySequence(Qt::Key_Backspace));

                QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this, SLOT(onDelete()));
            #else
                action->setShortcut(QKeySequence::Delete);
            #endif
			action->setData("delete");
			connect(action, SIGNAL(triggered()), this, SLOT(onCommand()));
			m_commands << action;
            m_delete = action;
		}

		m_editMenu->addSeparator();

		// Select All
		{
			QAction *action = m_editMenu->addAction(tr("&Select All", "Edit|Select All"));
			action->setShortcut(QKeySequence::SelectAll);
			action->setData("selectAll");
			connect(action, SIGNAL(triggered()), this, SLOT(onCommand()));
			m_commands << action;
		}
	}

	// view menu
	{
		m_viewMenu = menuBar()->addMenu(tr("&View"));

		QAction *zoomFit = m_viewMenu->addAction(tr("Zoom to &fit", "View|Zoom to Fit"), m_viewer, SLOT(zoomToFit()), QKeySequence("f"));
		QAction *zoom100 = m_viewMenu->addAction(tr("Zoom to 1:1", "View|Zoom to 1:1"), m_viewer, SLOT(zoom100()), QKeySequence("h"));
		QAction *zoomIn = m_viewMenu->addAction(tr("Zoom &in", "View|Zoom In"), m_viewer, SLOT(zoomIn()), QKeySequence("i"));
		QAction *zoomOut = m_viewMenu->addAction(tr("Zoom &out", "View|Zoom Out"), m_viewer, SLOT(zoomOut()), QKeySequence("o"));
	}

	// help menu
	{
		QMenu *menu = menuBar()->addMenu(tr("&Help"));

		menu->addAction(tr("&User Guide...", "Help|User Guide"), this, SLOT(onHelp()), QKeySequence("F1"));
		menu->addAction(tr("&About...", "Help|About"), this, SLOT(onAbout()));
	}

	updateMenus();
	updateUndo();
    onTab(0);
}

void MainWindow::loadCradle(const QString &path)
{
	QString imagePath = m_project->imagePath();
	if (path.isEmpty())
		m_project->clear();
	else
		m_project->load(path);
	m_project->setImagePath(imagePath);
    ImageManager::get().removeSource()->setProject(m_project);
    ImageManager::get().removeSource()->setIsFinal(true);

	m_tabs->setCurrentIndex(kTab_Mark);
	m_tools->setEnabled(true);
	m_viewerTools->setEnabled(true);
	m_tabs->setEnabled(true);
	onTab(m_tabs->currentIndex());
}

void MainWindow::open(const QString &path, bool pluginMode)
{
	m_pluginMode = pluginMode;
	onClose();

	QApplication::setOverrideCursor(Qt::BusyCursor);

	QString imagePath = path;
	ImageManager::get().load(imagePath);

	QApplication::restoreOverrideCursor();
    m_viewer->setTool(Viewer::kTool_Polygon);

	loadCradle(QString());
	m_project->setImagePath(imagePath);

	m_open->setVisible(!pluginMode);
	m_export->setVisible(!pluginMode);
	m_close->setVisible(!pluginMode);
	m_doneAction->setVisible(pluginMode);
	m_cancelAction->setVisible(pluginMode);
}

//
// SLOTS
//
void MainWindow::onOpenImage()
{
	QSettings settings;
	QString dir = settings.value("/imageDir").toString();
	QString filter = QString("*.tif;*.tiff;*.png").arg(kProjectExtension);
	QFileDialog dialog(this, tr("Open Image"), dir, filter);
	if (dialog.exec())
	{
		QStringList files = dialog.selectedFiles();
		if (files.size() == 1)
		{
			QString path = files[0];
			settings.setValue("/imageDir", QFileInfo(path).absolutePath());
			open(path);
		}
	}
}

void MainWindow::onOpen()
{
	QSettings settings;
	QString dir = settings.value("/imageDir").toString();
	QString filter = QString("*.%1").arg(kProjectExtension);
	QFileDialog dialog(this, tr("Load Cradle"), dir, filter);
	if (dialog.exec())
	{
		QStringList files = dialog.selectedFiles();
		if (files.size() == 1)
		{
			QString path = files[0];
			settings.setValue("/imageDir", QFileInfo(path).absolutePath());
			loadCradle(path);
		}
	}
}

void MainWindow::onClose()
{
	m_viewer->setTool(Viewer::kTool_None);
	m_tools->setEnabled(false);
	m_viewerTools->setEnabled(false);
    m_tabs->setEnabled(false);

    ImageManager::get().removeSource()->setProject(nullptr);

	m_project->clear();
	m_undoMgr->clear();
	m_clipboard->clear();
	ImageManager::get().clear();
}

void MainWindow::onSave()
{
	if (m_project->path().isEmpty())
		onSaveAs();
	else
		save(m_project->path());
}

bool MainWindow::saveAs()
{
	QSettings settings;
	QString dir = settings.value("/imageDir").toString();
	QString filter = QString("*.%1").arg(kProjectExtension);
	QString path = QFileDialog::getSaveFileName(this, tr("Save Cradle"), dir, filter);
	if (!path.isNull())
	{
		if (QFileInfo(path).suffix() != kProjectExtension)
			path = QString("%1.%2").arg(path).arg(kProjectExtension);
		settings.setValue("/imageDir", QFileInfo(path).absolutePath());
		save(path);
	}
	return !path.isNull();
}

void MainWindow::onSaveAs()
{
	saveAs();
}

void MainWindow::onExit()
{
	onClose();
	close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	ImageManager::get().clear();
    QMainWindow::closeEvent(event);
}

void MainWindow::onImageChanged()
{
	updateMenus();

    const ImageSource *source = ImageManager::get().source();
    if (source)
    {
        statusBar()->showMessage(QString("Image Size: %1x%2").arg(source->size().width()).arg(source->size().height()));
    }
}

void MainWindow::updateMenus()
{
	const ImageSource *source = ImageManager::get().source();

	m_close->setEnabled(source != nullptr);
	m_loadCradle->setEnabled(source != nullptr);
	m_save->setEnabled(source != nullptr);
	m_saveAs->setEnabled(source != nullptr);
	m_editMenu->setEnabled(source != nullptr);
	m_viewMenu->setEnabled(source != nullptr);
	m_export->setEnabled(source != nullptr);

    m_scroller->setEnabled(source != nullptr);
    m_removeCradle->setEnabled(!Project::activeProject()->polygons(Polygon::INPUT).empty());

	if (source)
	{
		const Manipulator *m = m_viewer->manipulator();
		for (auto cmd : m_commands)
		{
			QString name = cmd->data().toString();
			cmd->setEnabled(m && m->canDoCommand(name));
		}
	}

    if (m_tabs->currentIndex() == kTab_Remove)
    {
        auto polygons = Project::activeProject()->selection(Polygon::OUTPUT);
        m_black->setEnabled(!polygons.empty());
        m_gamma->setEnabled(!polygons.empty());
        m_white->setEnabled(!polygons.empty());
    }
    m_invert->setVisible(m_tabs->currentIndex() == kTab_Mark && m_viewer->tool() == Viewer::kTool_Mask);
    //m_detectCradle->setVisible(m_tabs->currentIndex() == kTab_Mark && m_viewer->tool() == Viewer::kTool_Polygon);
    //m_removeCradle->setVisible(m_tabs->currentIndex() == kTab_Mark && m_viewer->tool() == Viewer::kTool_Polygon);
}

void MainWindow::updateUndo()
{
	m_undo->setEnabled(m_undoMgr->canUndo());
	m_redo->setEnabled(m_undoMgr->canRedo());
	m_undo->setText(tr("Undo %1").arg(m_undoMgr->undoText()));
	m_redo->setText(tr("Redo %1").arg(m_undoMgr->redoText()));
}

void MainWindow::onUndoRedoChanged(bool can)
{
	updateUndo();
}

void MainWindow::onUndoRedoText(const QString &label)
{
	updateUndo();
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
	return QMainWindow::eventFilter(object, event);
}

void MainWindow::onToolChanged()
{
	Viewer::Tool tool = m_viewer->tool();
	for (auto action : m_toolActions)
		action->setChecked(action->data() == tool);

	updateMenus();
}

void MainWindow::onTool()
{
	QAction *action = qobject_cast<QAction *>(sender());
	Q_ASSERT(action);
	m_viewer->setTool(Viewer::Tool(action->data().toInt()));
}

void MainWindow::onToggleOverlay()
{
	m_viewer->enableOverlay(m_overlay->isChecked());
}

void MainWindow::onToggleResult()
{
	ImageManager::get().removeSource()->setBypass(!m_showResult->isChecked());
}

void MainWindow::onDelete()
{
    if (m_delete->isEnabled())
        m_viewer->manipulator()->doCommand("delete");
}

void MainWindow::onCommand()
{
	QString cmd = qobject_cast<QAction *>(sender())->data().toString();
	m_viewer->manipulator()->doCommand(cmd);
}

void MainWindow::onSelection()
{
	updateMenus();

    if (m_tabs->currentIndex() == kTab_Remove)
        updateSliders();
}

void MainWindow::onTab(int index)
{
	switch (index)
	{
		case kTab_Mark:
			m_viewer->setTool(Viewer::kTool_Polygon);
			break;
		case kTab_Remove:
			m_viewer->setTool(Viewer::kTool_Remove);
			break;
		case kTab_Texture:
			m_viewer->setTool(Viewer::kTool_Texture);
			break;
	}
	m_polygon->setVisible(index == kTab_Mark);
	m_mask->setVisible(index == kTab_Mark);
	m_detectCradle->setVisible(index == kTab_Mark);
	m_removeCradle->setVisible(index == kTab_Mark);
	m_removeTexture->setVisible(index == kTab_Texture);

	m_showResult->setVisible(index == kTab_Remove);
	m_black->setVisible(index == kTab_Remove);
	m_gamma->setVisible(index == kTab_Remove);
	m_white->setVisible(index == kTab_Remove);

    if (index == kTab_Remove)
        updateSliders();
}

static void buildMask(IplImage *result, const QImage &mask, unsigned char bitMask)
{
    for (int y = 0; y < result->height; y++)
    {
        uchar *drow = iplImageRow<uchar>(result, y);
        const uchar *srow = mask.scanLine(y);
        for (int x = 0; x < result->width; x++)
        {
            if (srow[x])
                drow[x] |= bitMask;
        }
    }
}

void MainWindow::onDetectCradle()
{
    DetectCradleDialog dialog(this);
    if (dialog.exec() == QDialog::Rejected)
        return;

    // render the defect mask
    QSize size = ImageManager::get().size();
    QImage maskImage(size, QImage::Format_Mono);
    maskImage.fill(0);
    m_project->renderMask(maskImage);
    maskImage = maskImage.convertToFormat(QImage::Format_Indexed8);

    // wrap it in OpenCV image and setr the DEFECT bit
    cvImageRef maskWrapper(cvCreateImage(cvSize(size.width(), size.height()), IPL_DEPTH_8U, 1));
    std::memset(maskWrapper->imageData, 0, maskWrapper->imageSize);
    buildMask(maskWrapper, maskImage, CradleFunctions::DEFECT);
    auto arr_to_mat = [](auto&& imageref) {
        return cv::cvarrToMat(imageref.get()).clone();
    };
    cv::Mat mask(arr_to_mat(maskWrapper));
    cv::Mat source(arr_to_mat(ImageManager::get().floatImage()));

    // run cradle detection
    std::vector<int> vrange;
    std::vector<int> hrange;
    QSize members = dialog.members();
    if (members.isValid())
        CradleFunctions::cradledetect(source, mask, members.height(), members.width(), vrange, hrange);
    else
        CradleFunctions::cradledetect(source, mask, vrange, hrange);

    // create polygons from the detected positions
    UndoManager::instance()->beginMacro(tr("Automatic Cradle Detection"));
    UndoManager::instance()->push(new RemovePolygonCommand(m_project->polygons(Polygon::INPUT)));

    size_t h_bars = hrange.size() / 2;
    size_t v_bars = vrange.size() / 2;

    // add horizontal bars
    for (size_t i = 0; i < h_bars; i++)
    {
        int y0 = hrange[i * 2];
        int y1 = hrange[i * 2 + 1];

        QRect rect(0, y0, size.width(), y1 - y0);
        PolygonPointer p(new Polygon(Polygon::INPUT));
        p->set(rect);
        UndoManager::instance()->push(new AddPolygonCommand(p));
    }
    // add vertical bars
    for (size_t i = 0; i < v_bars; i++)
    {
        int x0 = vrange[i * 2];
        int x1 = vrange[i * 2 + 1];

        QRect rect(x0, 0, x1 - x0, size.height());
        PolygonPointer p(new Polygon(Polygon::INPUT));
        p->set(rect);
        UndoManager::instance()->push(new AddPolygonCommand(p));
    }

    UndoManager::instance()->endMacro();

    m_viewer->setTool(Viewer::kTool_Polygon);
}

static cvImageRef buildMask(const Project *project)
{
	auto polygons = project->polygons();

	QSize size = ImageManager::get().size();
    cvImageRef mask(cvCreateImage(cvSize(size.width(), size.height()), IPL_DEPTH_8U, 1));
    std::memset(mask->imageData, 0, mask->imageSize);

	// render the defect mask
	{
		QImage maskImage(size, QImage::Format_Mono);
		maskImage.fill(0);
		project->renderMask(maskImage);
		maskImage = maskImage.convertToFormat(QImage::Format_Indexed8);
		buildMask(mask, maskImage, CradleFunctions::DEFECT);
	}

	// now add horizontal pieces
	{
		QImage maskImage(size, QImage::Format_Mono);
		maskImage.fill(0);
		QPainter painter(&maskImage);
		painter.setPen(Qt::NoPen);
		painter.setBrush(Qt::white);
		QPainterPath path;
		for (auto p : polygons)
		{
			if (p->type() == Polygon::INPUT && p->isHorizontal())
				path.addPath(p->path());
		}
		painter.drawPath(path);
		painter.end();
		maskImage = maskImage.convertToFormat(QImage::Format_Indexed8);
		buildMask(mask, maskImage, CradleFunctions::H_MASK);
	}

	// now add vertical pieces
	{
		QImage maskImage(size, QImage::Format_Mono);
		maskImage.fill(0);
		QPainter painter(&maskImage);
		painter.setPen(Qt::NoPen);
		painter.setBrush(Qt::white);
		QPainterPath path;
		for (auto p : polygons)
		{
			if (p->type() == Polygon::INPUT && !p->isHorizontal())
				path.addPath(p->path());
		}
		painter.drawPath(path);
		painter.end();
		maskImage = maskImage.convertToFormat(QImage::Format_Indexed8);
		buildMask(mask, maskImage, CradleFunctions::V_MASK);
	}

	return mask.detach();
}
void MainWindow::save(const QString &path)
{
	try
	{
		m_project->save(path);
		m_project->setPath(path);

        extern bool s_debug_mode;
        if (s_debug_mode)
        {
            auto arr_to_mat = [](auto&& imageref) {
              return cv::cvarrToMat(imageref.get()).clone();
            };

        	cvImageRef maskImage = buildMask(m_project);
            QString maskPath = path + ".mask.tiff";
            cv::imwrite(maskPath.toUtf8().constData(), cv::Mat(arr_to_mat(maskImage)));
        }
	}
	catch (const QString &msg)
	{
		QMessageBox::critical(this,
				QCoreApplication::applicationName(),
				tr("Could not create file '%1'.\n%2").arg(path).arg(msg),
				QMessageBox::Ok,
				QMessageBox::NoButton);
	}
}

static void buildPolygons(QList<QPolygon> &polygons, const QPainterPath &path)
{
    int i = 0;
    QPolygon poly;
    while (i < path.elementCount())
    {
        QPainterPath::Element e = path.elementAt(i);
        if (e.isMoveTo())
        {
            if (poly.count() != 0)
                polygons << poly;
            poly.clear();
            poly << QPoint(e.x, e.y);
        }
        else if (e.isLineTo())
            poly << QPoint(e.x, e.y);
        else if (e.isCurveTo())
        {
            poly << QPoint(e.x, e.y);
        }
        i++;
    }
    if (poly.count() != 0)
        polygons << poly;
}

class CradleCallbacks : public CradleFunctions::Callbacks
{
    MainWindow *m_window;
    int m_total;
    mutable int m_counter;
    static bool s_canceled;

public:
    CradleCallbacks(MainWindow *window, int total = -1) : m_window(window), m_total(total), m_counter(0)
    {
        s_canceled = false;
    }

    static void cancel()
    {
        s_canceled = true;
    }

    static bool isCanceled()
    {
        return s_canceled;
    }

    virtual bool progress(int value, int total) const
    {
        if (m_total > 0)
            m_window->progress(m_counter++, m_total);
        else
            m_window->progress(value, total);

        return !s_canceled;
    }
};

static uchar findMedian(const cv::Mat &sourceMat, const cv::Mat &resultMat, const cv::Mat &maskMat, int index)
{
    uchar result = 0;
    std::vector<uchar> vec;
    for (int y = 0; y < maskMat.rows; y++)
    {
        const float *srow = (const float *)sourceMat.row(y).ptr();
        const float *rrow = (const float *)resultMat.row(y).ptr();
        const short *mrow = (const short *)maskMat.row(y).ptr();
        const float *end = srow + maskMat.cols;
        while (srow != end)
        {
            if (*mrow == index)
            {
                int v = *srow - *rrow;
                v = v < 0 ? 0 : v > 255 ? 255 : v;
                vec.push_back(v);
            }
            srow++;
            rrow++;
            mrow++;
        }
    }
    std::sort(vec.begin(), vec.end());
    if (!vec.empty())
        result = vec[vec.size() / 2];
    return result;
}

static QLine extend(const QLine &input, const QRect &rect, Qt::Orientation o)
{
    if (o == Qt::Horizontal)
    {
        if (input.dy() == 0)
        {
            return QLine(0, input.y1(), rect.width(), input.y2());
        }
        else
        {
            float m = float(input.dy()) / float(input.dx());
            float c = input.p1().y() - m * input.p1().x();

            float y1 = m * 0.0f + c;
            float y2 = m * rect.width() + c;

            return QLineF(0.0, y1, rect.width(), y2).toLine();
        }
    }
    else
    {
        if (input.dx() == 0)
        {
            return QLine(input.x1(), 0, input.x2(), rect.height());
        }
        else
        {
            float m = float(input.dx()) / float(input.dy());
            float c = input.p1().x() - m * input.p1().y();

            float x1 = m * 0.0f + c;
            float x2 = m * rect.height() + c;

            return QLineF(x1, 0.0f, x2, rect.height()).toLine();
        }
    }
    return input;
}

static Polygon::PointList extend(const Polygon::PointList &input, const QRect &rect, Qt::Orientation o)
{
    Polygon::PointList output;

    if (o == Qt::Horizontal)
    {
        QPoint p0 = input[0].knot;
        QPoint p1 = input[1].knot;
        QPoint p2 = input[3].knot;
        QPoint p3 = input[2].knot;

        p0.setY(std::max(p0.y(), 0));
        p1.setY(std::max(p1.y(), 0));
        p2.setY(std::min(p2.y(), rect.height()));
        p3.setY(std::min(p3.y(), rect.height()));

        QLine line1 = extend(QLine(p0, p1), rect, o);
        QLine line2 = extend(QLine(p2, p3), rect, o);
        if (line1.dx() > 5 && line2.dx() > 5)
        {
            output << line1.p1() << line1.p2();
            output << line2.p2() << line2.p1();
        }
    }
    else
    {
        QPoint p0 = input[0].knot;
        QPoint p1 = input[3].knot;
        QPoint p2 = input[1].knot;
        QPoint p3 = input[2].knot;

        p0.setX(std::max(p0.x(), 0));
        p1.setX(std::max(p1.x(), 0));
        p2.setX(std::min(p2.x(), rect.width()));
        p3.setX(std::min(p3.x(), rect.width()));

        QLine line1 = extend(QLine(p0, p1), rect, o);
        QLine line2 = extend(QLine(p2, p3), rect, o);
        if (line1.dy() > 5 && line2.dy() > 5)
        {
            output << line1.p1() << line2.p1();
            output << line2.p2() << line1.p2();
        }
    }

    return output;
}

bool CradleCallbacks::s_canceled;

void MainWindow::onRemoveCradle()
{
    const cvImageRef &source = ImageManager::get().floatImage();
    const cvImageRef &result = ImageManager::get().resultImage();
    const cvImageRef &removeMask = ImageManager::get().removeMask();

    ImageManager::get().removeSource()->setIsFinal(false);

	cvImageRef maskImage = buildMask(m_project);

    UndoManager::instance()->beginMacro(tr("Generate Removal Segments"));

    // remove old output polygons
    UndoManager::instance()->push(new RemovePolygonCommand(Project::activeProject()->polygons(Polygon::OUTPUT)));

	// build mid-point vectors for all input polygons
	std::vector<std::vector<int>> h_midpoints;
	std::vector<std::vector<int>> v_midpoints;
	std::vector<int> h_s;
	std::vector<int> v_s;
	std::vector<std::vector<std::vector<float>>> h_vm;
	std::vector<std::vector<std::vector<float>>> v_vm;

    QPainterPath hPaths, vPaths;

    QRect rect(0, 0, source->width, source->height);

	auto input_polygons = Project::activeProject()->polygons(Polygon::INPUT);
	for (auto poly : input_polygons)
	{
		if (poly->isHorizontal())
		{
            // extend to edges and clip
            Polygon::PointList points = extend(poly->points(), rect, Qt::Horizontal);
            if (points.empty())
                UndoManager::instance()->push(new RemovePolygonCommand(poly));
            else
            {
                UndoManager::instance()->push(new EditPolygonCommand(poly, points));

                QPainterPath path = poly->path();
                hPaths += path;
                QLine line = poly->centerLine();
                QRect boundingRect = path.controlPointRect().toRect();

                std::vector<int> endPoints;
                endPoints.push_back(line.x1());
                endPoints.push_back(line.y1());
                endPoints.push_back(line.x2());
                endPoints.push_back(line.y2());
                h_midpoints.push_back(endPoints);
                h_s.push_back(boundingRect.height());
            }
		}
		else
		{
            // extend to edges and clip
            Polygon::PointList points = extend(poly->points(), rect, Qt::Vertical);
            if (points.empty())
                UndoManager::instance()->push(new RemovePolygonCommand(poly));
            else
            {
                UndoManager::instance()->push(new EditPolygonCommand(poly, points));

                QPainterPath path = poly->path();
                vPaths += path;
                QLine line = poly->centerLine();
                QRect boundingRect = path.controlPointRect().toRect();

                std::vector<int> endPoints;
                endPoints.push_back(line.y1());
                endPoints.push_back(line.x1());
                endPoints.push_back(line.y2());
                endPoints.push_back(line.x2());
                v_midpoints.push_back(endPoints);
                v_s.push_back(boundingRect.width());
            }
		}
	}

    // clear out result image
	std::memset(result->imageData, 0, result->imageSize);
	std::memset(removeMask->imageData, 0, removeMask->imageSize);
            auto arr_to_mat = [](auto&& imageref) {
              return cv::cvarrToMat(imageref.get()).clone();
            };
	cv::Mat sourceMat(arr_to_mat(source));
	cv::Mat resultMat(arr_to_mat(result));
    cv::Mat maskMat(arr_to_mat(maskImage));

    // set up progress handler
    int total = int(h_s.size() + v_s.size() + h_s.size() * v_s.size());
    CradleCallbacks progress(this, total);
    CradleFunctions::setCallbacks(&progress);

    beginProgress(tr("Removing Cradle..."));

    // jump to Remove tab so progress can be seen
	m_tabs->setCurrentIndex(kTab_Remove);

    // remove!
    CradleFunctions::MarkedSegments ms;
    ms.pieces = 0;
    ms.piece_mask = cv::Mat(arr_to_mat(removeMask));
	ms.pieceIDh.resize(h_s.size());
	ms.pieceIDv.resize(v_s.size());

 	CradleFunctions::removeHorizontal(sourceMat, maskMat, resultMat,
			h_midpoints, h_s, h_vm, ms);
    if (!progress.isCanceled())
    {
        CradleFunctions::removeVertical(sourceMat, maskMat, resultMat,
                v_midpoints, v_s, v_vm, ms);
    }

    if (!progress.isCanceled())
    {
        CradleFunctions::removeCrossSection(sourceMat, maskMat, resultMat,
                h_s, v_s,
                h_midpoints, v_midpoints, h_vm, v_vm, ms);
    }

    ImageManager::get().removeSource()->invalidate();

    // build output polygons by intersecting all of the input polygons
    {
        QPainterPath subH = hPaths.subtracted(vPaths);
        QPainterPath subV = vPaths.subtracted(hPaths);
        QPainterPath intersected = vPaths.intersected(hPaths);

        QList<QPolygon> output_polygons;
        buildPolygons(output_polygons, subH);
        buildPolygons(output_polygons, subV);
        buildPolygons(output_polygons, intersected);

        for (auto poly : output_polygons)
        {
            PolygonPointer p(new Polygon(Polygon::OUTPUT));
            p->set(poly);

            int index = -1;
            uchar median = 0;
            QRect rect = poly.boundingRect();

            // found out what output mask index this is
            for (int i = 0; i < ms.pieces && index < 0; i++)
            {
                cv::Point p = ms.piece_middle[i];
                // +++ COORDINATES ARE REVERSED!!! +++
                if (rect.contains(QPoint(p.y, p.x)))
                {
                    index = i;
                    median = findMedian(sourceMat, resultMat, ms.piece_mask, index);
                }
            }
            p->setValue("index", index);
            p->setValue("median", median);
            p->setValue("black", 0);
            p->setValue("white", 255);
            p->setValue("gamma", 0);

            UndoManager::instance()->push(new AddPolygonCommand(p));
        }
    }

    CradleFunctions::setCallbacks(nullptr);
    endProgress();

    UndoManager::instance()->endMacro();

    Project::activeProject()->setMarkedSegments(ms);
}

void MainWindow::progress(int value, int total) const
{
    m_progressBar->setMaximum(total - 1);
    m_progressBar->setValue(value);
    ImageManager::get().removeSource()->invalidate();
    QApplication::processEvents();
}

void MainWindow::beginProgress(const QString &msg)
{
	QApplication::setOverrideCursor(Qt::BusyCursor);

    statusBar()->showMessage(msg);
    m_progressBar->show();
    m_cancel->show();
    m_progressBar->setValue(0);
}

void MainWindow::endProgress()
{
    m_progressBar->hide();
    m_cancel->hide();
    onImageChanged();

	QApplication::restoreOverrideCursor();
}

void MainWindow::onCancelProgress()
{
    CradleCallbacks::cancel();
}

void MainWindow::onReset()
{
    m_tabs->setCurrentIndex(kTab_Mark);
    UndoManager::instance()->beginMacro(tr("Reset"));
    UndoManager::instance()->push(new RemovePolygonCommand(Project::activeProject()->polygons()));
    UndoManager::instance()->endMacro();
}

void MainWindow::onPolygonAdded(Polygon *poly)
{
    updateMenus();
}

void MainWindow::onPolygonRemoved(Polygon *poly)
{
    updateMenus();
}

void MainWindow::onPolygonValueChanged(Polygon *poly, const QString &key)
{
    updateSliders();
}

bool MainWindow::doExport()
{
    const char *kExtension = "tiff";
	QSettings settings;
	QString dir = settings.value("/imageDir").toString();
	QString filter = QString("*.%1").arg(kExtension);
	QString path = QFileDialog::getSaveFileName(this, tr("Export Result"), dir, filter);
	if (!path.isNull())
	{
		if (QFileInfo(path).suffix() != kExtension)
			path = QString("%1.%2").arg(path).arg(kExtension);
		settings.setValue("/imageDir", QFileInfo(path).absolutePath());

        const ImageSource *source = ImageManager::get().removeSource();
        QSize size = source->size();
        QRect tile(QPoint(0, 0), size);
        QImage result = source->getTile(tile, size);
        result.save(path, "TIFF");
    }
	return !path.isNull();
}

void MainWindow::onExport()
{
	doExport();
}

void MainWindow::onSlider(int value)
{
    Slider *slider = qobject_cast<Slider *>(sender());
    Q_ASSERT(slider);
    QString key = slider->objectName();
    auto polygons = Project::activeProject()->selection(Polygon::OUTPUT);
    for (auto p : polygons)
    {
        if (slider->isIndeterminate())
        {
            int old = p->value(QString("backup.") + key).toInt();
            int new_value = value + old;
            int minV = slider->minimum();
            int maxV = slider->maximum();
            new_value = new_value < minV ? minV : new_value > maxV ? maxV : new_value;
            p->setValue(key, new_value);
        }
        else
            p->setValue(key, value);
    }
}

void MainWindow::onBeginChange()
{
    Slider *slider = qobject_cast<Slider *>(sender());
    Q_ASSERT(slider);
    QString key = slider->objectName();

    if (m_viewer->manipulator())
        m_viewer->manipulator()->beginParameterChange(key);

    // back up current value
    auto polygons = Project::activeProject()->selection(Polygon::OUTPUT);
    for (auto p : polygons)
        p->setValue(QString("backup.") + key, p->value(key));
}

void MainWindow::onEndChange()
{
    Slider *slider = qobject_cast<Slider *>(sender());
    Q_ASSERT(slider);
    QString key = slider->objectName();

    // restore backups
    UndoManager::instance()->beginMacro(tr("Edit Levels"));
    auto polygons = Project::activeProject()->selection(Polygon::OUTPUT);
    for (auto p : polygons)
    {
        // save new value
        int value = p->value(key).toInt();
        // restore the original value
        p->setValue(key, p->value(QString("backup.") + key));

        // and set new value
        UndoManager::instance()->push(new EditPolygonCommand(p, key, value));
    }
    UndoManager::instance()->endMacro();

    if (m_viewer->manipulator())
        m_viewer->manipulator()->endParameterChange(key);

    updateSliders();
}

void MainWindow::updateSliders()
{
    QVector<int> black;
    QVector<int> gamma;
    QVector<int> white;
    auto polygons = Project::activeProject()->selection(Polygon::OUTPUT);
    for (auto p : polygons)
    {
        black << p->value("black").toInt();
        gamma << p->value("gamma").toInt();
        white << p->value("white").toInt();
    }
    m_black->setValues(black);
    m_gamma->setValues(gamma);
    m_white->setValues(white);
}

static bool s_canceled = false;

void MainWindow::onDone()
{
    s_canceled = false;

    RemoveSource *source = ImageManager::get().removeSource();
	source->setBypass(false);
    QSize size = source->size();
    QRect tile(QPoint(0, 0), size);
    QImage result = source->getTile(tile, size);
	QString path = m_project->imagePath();
	QFile::remove(path);
    if (result.save(path, "TIFF"))
		close();
}

void MainWindow::onCancel()
{
    s_canceled = true;
    close();
}

bool MainWindow::canceled() const
{
    return s_canceled;
}

void MainWindow::onRemoveTexture()
{
	if (m_pluginMode)
	{
		if (QMessageBox::question(this, tr("Remove Texture"), tr("Texture removal can take a very long time. Continue?")) == QMessageBox::No)
			return;
	}
	else
	{
		QMessageBox::StandardButton result = QMessageBox::question(this,
				tr("Remove Texture"),
				tr("Texture removal can take a very long time. Save intermediate result before continuing??"),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (result == QMessageBox::Cancel)
			return;
		if (result == QMessageBox::Yes)
		{
			if (!saveAs())
				return;
		}
	}

    // grab the current result and convert to float
    RemoveSource *source = ImageManager::get().removeSource();
	source->setBypass(false);
    QSize size = source->size();
    QRect tile(QPoint(0, 0), size);
    QImage resultImage = source->getTile(tile, size);

    cv::Mat resultWrap(size.height(), size.width(), CV_8U, (void *)resultImage.bits(), resultImage.bytesPerLine());

    const cvImageRef &floatResult = ImageManager::get().resultImage();
    cv::Mat resultMat(size.height(), size.width(), CV_32F, (void *)iplImageRow<float>(floatResult, 0), floatResult->widthStep);
    resultWrap.convertTo(resultMat, CV_32F);
    source->setIsFinal(true);
            auto arr_to_mat = [](auto&& imageref) {
              return cv::cvarrToMat(imageref.get()).clone();
            };
    cvImageRef mask = buildMask(m_project);
    cv::Mat maskMat(arr_to_mat(mask));

    cv::Mat outMat;

    CradleCallbacks progress(this);
    CradleFunctions::setCallbacks(&progress);

    beginProgress(tr("Removing Texture..."));

    try
    {
        TextureRemoval::textureRemove(resultMat, maskMat, outMat, Project::activeProject()->markedSegments());
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(this, QCoreApplication::applicationName(), e.what());
    }

    outMat.copyTo(resultMat);
    source->invalidate();

    CradleFunctions::setCallbacks(nullptr);
    endProgress();
}

void MainWindow::onHelp()
{
    QString appPath = QCoreApplication::applicationDirPath();
    #if defined(Q_OS_MACX)
        appPath = QFileInfo(appPath).absolutePath();
        appPath = QFileInfo(appPath).absolutePath();
		appPath = QFileInfo(appPath).absolutePath();
    #endif
    QString helpPath = QDir(appPath).absoluteFilePath("User Guide.pdf");
    QDesktopServices::openUrl(QUrl::fromLocalFile(helpPath));
}

void MainWindow::onAbout()
{
    QMessageBox info(this);
    info.setWindowTitle(tr("About %1").arg(QCoreApplication::applicationName()));
    info.setIconPixmap(QPixmap(":/images/icon.png"));
    info.setText(tr("%1 v%2").arg(QCoreApplication::applicationName()).arg(QCoreApplication::applicationVersion()));
    info.setInformativeText(tr("Copyright (c) 2015 Duke University"));
    info.exec();
}
