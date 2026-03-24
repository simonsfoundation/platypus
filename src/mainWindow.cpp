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
#include <textureRemovalStatus.h>
#include <util.hpp>
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
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QFrame>
#include <QShortcut>
#include <QtWidgets/QBoxLayout>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QDesktopServices>
#include <QtGui/QStatusTipEvent>
#include <QtWidgets/QStyle>
#include <QtCore/QSettings>
#include <QtCore/QLine>

static const char *kProjectExtension = "platypus";

namespace
{
QColor blendColors(const QColor &a, const QColor &b, qreal factor)
{
    const qreal inverse = 1.0 - factor;
    return QColor::fromRgbF(a.redF() * inverse + b.redF() * factor,
                            a.greenF() * inverse + b.greenF() * factor,
                            a.blueF() * inverse + b.blueF() * factor,
                            a.alphaF() * inverse + b.alphaF() * factor);
}

QString cssColor(const QColor &color)
{
    return color.name(QColor::HexArgb);
}

QString plainActionText(const QAction *action)
{
    QString text = action->iconText();
    if (text.isEmpty())
        text = action->text();
    int tabIndex = text.indexOf('\t');
    if (tabIndex >= 0)
        text.truncate(tabIndex);
    text.remove('&');
    return text.trimmed();
}

QString effectiveActionToolTip(const QAction *action)
{
    const QString toolTip = action->toolTip().trimmed();
    return toolTip.isEmpty() ? plainActionText(action) : toolTip;
}

QString effectiveActionStatusTip(const QAction *action)
{
    const QString statusTip = action->statusTip().trimmed();
    return statusTip.isEmpty() ? effectiveActionToolTip(action) : statusTip;
}

void applyHelpText(QWidget *widget,
                   const QString &toolTip,
                   const QString &statusTip,
                   const QString &accessibleName = QString())
{
    widget->setToolTip(toolTip);
    widget->setStatusTip(statusTip);
    widget->setWhatsThis(statusTip);
    widget->setAccessibleName(accessibleName.isEmpty() ? toolTip : accessibleName);
    widget->setAccessibleDescription(statusTip);
}

class HoverStatusTipFilter : public QObject
{
public:
    explicit HoverStatusTipFilter(QWidget *owner) : QObject(owner)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (!widget)
            return QObject::eventFilter(watched, event);

        if (event->type() == QEvent::Enter) {
            const QString statusTip = widget->statusTip();
            if (!statusTip.isEmpty()) {
                QStatusTipEvent statusEvent(statusTip);
                QApplication::sendEvent(widget->window(), &statusEvent);
            }
        } else if (event->type() == QEvent::Leave) {
            QStatusTipEvent clearEvent(QString{});
            QApplication::sendEvent(widget->window(), &clearEvent);
        }

        return QObject::eventFilter(watched, event);
    }
};

void installHoverStatusTip(QWidget *widget)
{
    widget->installEventFilter(new HoverStatusTipFilter(widget));
}

QString mainWindowStyleSheet(const QPalette &palette)
{
    const QColor window = palette.color(QPalette::Window);
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::WindowText);
    const QColor highlight = palette.color(QPalette::Highlight);
    const bool dark = window.lightness() < 140;

    const QColor chrome = blendColors(window, base, dark ? 0.10 : 0.36);
    const QColor chromeBorder = blendColors(text, window, dark ? 0.86 : 0.93);
    const QColor contextPanel = blendColors(base, window, dark ? 0.16 : 0.06);
    const QColor contextBorder = blendColors(text, window, dark ? 0.88 : 0.94);
    const QColor toneTitle = blendColors(text, window, dark ? 0.30 : 0.46);
    const QColor toneTrack = blendColors(text, window, dark ? 0.86 : 0.92);
    const QColor toneInactive = blendColors(base, window, dark ? 0.18 : 0.10);
    const QColor toneHandle = dark ? QColor(245, 245, 247) : QColor(255, 255, 255);
    QColor hoverFill = blendColors(highlight, chrome, dark ? 0.78 : 0.64);
    QColor selectedFill = highlight;
    selectedFill.setAlpha(dark ? 52 : 28);
    hoverFill.setAlpha(dark ? 36 : 24);

    return QStringLiteral(R"(
QWidget#mainWindowCentral {
    background: %1;
}

QWidget#primaryBar {
    background: %2;
    border-bottom: 1px solid %3;
}

QWidget#headerDivider {
    min-width: 10px;
    max-width: 10px;
    border-left: 1px solid %3;
    margin: 6px 2px;
}

QToolButton#primaryButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 7px;
    padding: 3px 8px;
    min-height: 24px;
    font-size: 12px;
    font-weight: 600;
}

QToolButton#primaryButton:hover {
    background: %9;
    border-color: %5;
}

QToolButton#primaryButton:checked {
    background: %8;
    border-color: %7;
}

QWidget#modeStrip {
    background: %2;
    border-bottom: 1px solid %3;
}

QStackedWidget#contextStack {
    background: transparent;
}

QTabBar#modeTabs::tab {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    margin: 0 4px 0 0;
    padding: 3px 10px;
    min-height: 24px;
    font-size: 12px;
    font-weight: 600;
}

QTabBar#modeTabs::tab:selected {
    background: %8;
    border-color: %7;
}

QTabBar#modeTabs::tab:hover:!selected {
    background: %9;
}

QToolButton#contextButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 3px 10px;
    min-height: 24px;
    font-size: 12px;
    font-weight: 600;
}

QToolButton#contextButton:hover {
    background: %9;
    border-color: %5;
}

QToolButton#contextButton:checked {
    background: %8;
    border-color: %7;
}

QWidget#tonePanel {
    background: transparent;
}

QWidget#tonePanel QLabel#tonePanelTitle {
    color: %6;
    font-size: 11px;
    font-weight: 600;
    margin-right: 2px;
}

QWidget#tonePanel QLabel#toneSliderLabel {
    color: %6;
    font-size: 11px;
    font-weight: 600;
}

QWidget#tonePanel QLineEdit#toneSliderValue {
    padding: 2px 6px;
    border: 1px solid %5;
    border-radius: 7px;
    background: %1;
    min-height: 20px;
}

QWidget#tonePanel QLineEdit#toneSliderValue:focus {
    border-color: %7;
}

QWidget#tonePanel QSlider#toneSliderControl {
    min-height: 18px;
}

QWidget#tonePanel QSlider#toneSliderControl::groove:horizontal {
    height: 4px;
    border-radius: 2px;
    background: %10;
    margin: 0 6px;
}

QWidget#tonePanel QSlider#toneSliderControl::sub-page:horizontal {
    background: %7;
    border-radius: 2px;
}

QWidget#tonePanel QSlider#toneSliderControl::add-page:horizontal {
    background: %11;
    border-radius: 2px;
}

QWidget#tonePanel QSlider#toneSliderControl::handle:horizontal {
    width: 12px;
    margin: -5px 0;
    border-radius: 6px;
    border: 1px solid %5;
    background: %12;
}

QWidget#tonePanel QSlider#toneSliderControl::handle:horizontal:hover {
    border-color: %7;
}

QStatusBar {
    border-top: 1px solid %3;
    background: %2;
}

QStatusBar::item {
    border: none;
}

QProgressBar#statusProgress {
    border: 1px solid %3;
    border-radius: 6px;
    min-height: 12px;
    text-align: center;
}

QProgressBar#statusProgress::chunk {
    background: %7;
    border-radius: 5px;
}

QPushButton#statusCancelButton {
    border-radius: 6px;
    padding: 2px 8px;
}

QPushButton#statusCancelButton:hover {
    background: %8;
}
)")
        .arg(cssColor(window),
             cssColor(chrome),
             cssColor(chromeBorder),
             cssColor(contextPanel),
             cssColor(contextBorder),
             cssColor(toneTitle),
             cssColor(highlight),
             cssColor(selectedFill),
             cssColor(hoverFill),
             cssColor(toneTrack),
             cssColor(toneInactive),
             cssColor(toneHandle));
}

void applyMacCompactSize(QWidget *widget, bool mini = false)
{
#if defined(Q_OS_MACOS)
    widget->setAttribute(mini ? Qt::WA_MacMiniSize : Qt::WA_MacSmallSize);
#else
    Q_UNUSED(widget);
    Q_UNUSED(mini);
#endif
}

QWidget *createDivider(QWidget *parent = nullptr)
{
    QWidget *divider = new QWidget(parent);
    divider->setObjectName("headerDivider");
    return divider;
}

QToolButton *createActionButton(QAction *action,
                                bool primary,
                                QWidget *parent = nullptr)
{
    QToolButton *button = new QToolButton(parent);
    button->setObjectName(primary ? QStringLiteral("primaryButton")
                                  : QStringLiteral("contextButton"));
    button->setDefaultAction(action);
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    applyHelpText(button,
                  effectiveActionToolTip(action),
                  effectiveActionStatusTip(action),
                  plainActionText(action));
    installHoverStatusTip(button);
#if defined(Q_OS_MACOS)
    applyMacCompactSize(button, primary);
    button->setIconSize(primary ? QSize(16, 16) : QSize(14, 14));
#else
    button->setIconSize(QSize(18, 18));
    applyMacCompactSize(button);
#endif
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setText(plainActionText(action));
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return button;
}

QWidget *createPrimaryGroup(std::initializer_list<QAction *> actions,
                            QWidget *parent = nullptr)
{
    QWidget *group = new QWidget(parent);
    QHBoxLayout *layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    for (QAction *action : actions)
        layout->addWidget(createActionButton(action, true, group));
    return group;
}

QWidget *createActionRow(const char *name,
                         std::initializer_list<QAction *> actions,
                         QWidget *parent = nullptr)
{
    QWidget *row = new QWidget(parent);
    row->setObjectName(QString::fromLatin1(name));

    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    for (QAction *action : actions)
        layout->addWidget(createActionButton(action, false, row));
    row->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    return row;
}
}

MainWindow::MainWindow(Project *project, QWidget *parent) : QMainWindow(parent),
	m_project(project),
	m_undoMgr(new UndoManager(this)),
	m_clipboard(new Clipboard(this)),
	m_refreshingTheme(false),
    m_pendingPluginMode(false),
	m_pluginMode(false)
{
    setObjectName("mainWindow");
    setAttribute(Qt::WA_AlwaysShowToolTips);

	connect(&ImageManager::get(), &ImageManager::imageChanged, this, &MainWindow::onImageChanged);
    connect(&ImageManager::get(), &ImageManager::loadFailed, this, &MainWindow::onImageLoadFailed);
	connect(&ImageManager::get(), &ImageManager::status, this,
            [this](const QString &msg) { statusBar()->showMessage(msg); });
	connect(m_undoMgr, &UndoManager::canRedoChanged, this, &MainWindow::onUndoRedoChanged);
	connect(m_undoMgr, &UndoManager::canUndoChanged, this, &MainWindow::onUndoRedoChanged);
	connect(m_undoMgr, &UndoManager::redoTextChanged, this, &MainWindow::onUndoRedoText);
	connect(m_undoMgr, &UndoManager::undoTextChanged, this, &MainWindow::onUndoRedoText);
	connect(m_clipboard, &Clipboard::changed, this, &MainWindow::updateMenus);

	connect(Project::activeProject(), &Project::selectionChanged, this, &MainWindow::onSelection);
	connect(Project::activeProject(), &Project::polygonAdded, this, &MainWindow::onPolygonAdded);
	connect(Project::activeProject(), &Project::polygonRemoved, this, &MainWindow::onPolygonRemoved);
	connect(Project::activeProject(), &Project::polygonValueChanged, this, &MainWindow::onPolygonValueChanged);

	m_viewer = new Viewer;

	connect(m_viewer, &Viewer::toolChanged, this, &MainWindow::onToolChanged);

	m_scroller = new ViewerScroller(m_viewer);
    m_scroller->setObjectName("viewerStage");

    // VIEWER TOOLS
    {
        QAction *reset = new QAction(QIcon(":images/reset.png"), tr("Reset"), this);
        connect(reset, &QAction::triggered, this, &MainWindow::onReset);
        reset->setToolTip(tr("Reset All"));
        reset->setStatusTip(tr("Reset all cradle polygons"));

        QAction *zoom100 = new QAction(QIcon(":images/1to1.png"), tr("Zoom 100%"), this);
        zoom100->setIconText(tr("1:1"));
        connect(zoom100, &QAction::triggered, m_viewer, &Viewer::zoom100);
        zoom100->setToolTip(tr("Zoom 1:1 (h)"));
        zoom100->setStatusTip(tr("View image at 100%%"));

        QAction *zoomFit = new QAction(QIcon(":images/fitImage.png"), tr("Zoom to Fit"), this);
        zoomFit->setIconText(tr("Fit"));
        connect(zoomFit, &QAction::triggered, m_viewer, &Viewer::zoomToFit);
        zoomFit->setToolTip(tr("Zoom to Fit (f)"));
        zoomFit->setStatusTip(tr("Fit image to the available workspace"));

        QAction *zoomIn = new QAction(QIcon(":images/zoomIn.png"), tr("Zoom In"), this);
        zoomIn->setIconText(tr("In"));
        connect(zoomIn, &QAction::triggered, m_viewer, &Viewer::zoomIn);
        zoomIn->setToolTip(tr("Zoom In (i)"));
        zoomIn->setStatusTip(tr("Zoom into the image"));

        QAction *zoomOut = new QAction(QIcon(":images/zoomOut.png"), tr("Zoom Out"), this);
        zoomOut->setIconText(tr("Out"));
        connect(zoomOut, &QAction::triggered, m_viewer, &Viewer::zoomOut);
        zoomOut->setToolTip(tr("Zoom Out (o)"));
        zoomOut->setStatusTip(tr("Zoom out from the image"));

        #if defined(Q_OS_MACOS)
            zoom100->setShortcut(QKeySequence("h"));
            zoomFit->setShortcut(QKeySequence("f"));
            zoomIn->setShortcut(QKeySequence("i"));
            zoomOut->setShortcut(QKeySequence("o"));
        #endif

        m_overlay = new QAction(QIcon(":images/overlay.png"), tr("Overlay"), this);
        connect(m_overlay, &QAction::triggered, this, &MainWindow::onToggleOverlay);
        m_overlay->setToolTip(tr("Toggle Overlay (q)"));
        m_overlay->setCheckable(true);
        m_overlay->setChecked(m_viewer->overlayEnabled());
        m_overlay->setShortcut(QKeySequence("q"));
        m_overlay->setStatusTip(tr("Show or hide the editing overlay"));

        m_doneAction = new QAction(QIcon(":images/done.png"), tr("Done"), this);
        connect(m_doneAction, &QAction::triggered, this, &MainWindow::onDone);
        m_doneAction->setToolTip(tr("Done"));
        m_doneAction->setStatusTip(tr("Apply changes and close"));
        m_cancelAction = new QAction(QIcon(":images/cancel.png"), tr("Cancel"), this);
        connect(m_cancelAction, &QAction::triggered, this, &MainWindow::onCancel);
        m_cancelAction->setToolTip(tr("Cancel"));
        m_cancelAction->setStatusTip(tr("Discard changes and close"));
        for (QAction *action :
             {reset, zoom100, zoomFit, zoomIn, zoomOut, m_overlay,
              m_doneAction, m_cancelAction}) {
            addAction(action);
        }

        m_primaryBar = new QWidget(this);
        m_primaryBar->setObjectName("primaryBar");
        m_primaryBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        {
            QHBoxLayout *layout = new QHBoxLayout(m_primaryBar);
            layout->setContentsMargins(12, 7, 12, 7);
            layout->setSpacing(6);
            layout->addWidget(createPrimaryGroup({reset}, m_primaryBar), 0, Qt::AlignVCenter);
            layout->addWidget(createDivider(m_primaryBar));
            layout->addWidget(createPrimaryGroup({zoom100, zoomFit, zoomIn, zoomOut}, m_primaryBar),
                              0,
                              Qt::AlignVCenter);
            layout->addWidget(createDivider(m_primaryBar));
            layout->addWidget(createPrimaryGroup({m_overlay}, m_primaryBar), 0, Qt::AlignVCenter);
            layout->addStretch(1);

            m_pluginControls = new QWidget(m_primaryBar);
            QHBoxLayout *pluginLayout = new QHBoxLayout(m_pluginControls);
            pluginLayout->setContentsMargins(0, 0, 0, 0);
            pluginLayout->setSpacing(8);
            pluginLayout->addWidget(createDivider(m_pluginControls));
            pluginLayout->addWidget(createActionButton(m_doneAction, true, m_pluginControls));
            pluginLayout->addWidget(createActionButton(m_cancelAction, true, m_pluginControls));
            m_pluginControls->hide();
            layout->addWidget(m_pluginControls, 0, Qt::AlignVCenter);
        }
    }

	// EDIT
	m_polygon = new QAction(QIcon(":images/polygon.png"), tr("Polygons"), this);
    m_polygon->setIconText(tr("Polygon"));
	connect(m_polygon, &QAction::triggered, this, &MainWindow::onTool);
	m_polygon->setToolTip(tr("Polygons (e)"));
	m_polygon->setShortcut(QKeySequence("e"));
	m_polygon->setCheckable(true);
	m_polygon->setData(Viewer::kTool_Polygon);
    m_polygon->setStatusTip(tr("Edit cradle polygons"));

	// MASK
	m_mask = new QAction(QIcon(":images/mask.png"), tr("Mask"), this);
	connect(m_mask, &QAction::triggered, this, &MainWindow::onTool);
	m_mask->setToolTip(tr("Mask (m)"));
	m_mask->setShortcut(QKeySequence("m"));
	m_mask->setCheckable(true);
	m_mask->setData(Viewer::kTool_Mask);
    m_mask->setStatusTip(tr("Edit defect mask"));

	m_toolActions << m_polygon;
	m_toolActions << m_mask;

	// INVERT
	m_invert = new QAction(QIcon(":images/invert.png"), tr("Invert"), this);
	connect(m_invert, &QAction::triggered, this, &MainWindow::onCommand);
	m_invert->setToolTip(tr("Invert Mask (Ctrl+i)"));
	m_invert->setShortcut(QKeySequence("Ctrl+i"));
	m_invert->setData("invertMask");
    m_invert->setStatusTip(tr("Invert mask shape"));
    m_commands << m_invert;

    // DETECT CRADLE
	m_detectCradle = new QAction(QIcon(":images/detectCradle.png"), tr("Detect Cradle"), this);
    m_detectCradle->setIconText(tr("Detect"));
	connect(m_detectCradle, &QAction::triggered, this, &MainWindow::onDetectCradle);
    m_detectCradle->setToolTip(tr("Detect Cradle"));
    m_detectCradle->setStatusTip(tr("Detect cradle"));

	m_removeCradle = new QAction(QIcon(":images/removeCradle.png"), tr("Remove Cradle"), this);
    m_removeCradle->setIconText(tr("Remove"));
	connect(m_removeCradle, &QAction::triggered, this, &MainWindow::onRemoveCradle);
    m_removeCradle->setToolTip(tr("Remove Cradle"));
    m_removeCradle->setStatusTip(tr("Remove cradle"));

	m_removeTexture = new QAction(tr("Remove Texture"), this);
    m_removeTexture->setIconText(tr("Texture"));
	connect(m_removeTexture, &QAction::triggered, this, &MainWindow::onRemoveTexture);
    m_removeTexture->setToolTip(tr("Remove Texture"));
    m_removeTexture->setStatusTip(tr("Remove texture"));

	// REMOVAL TOOLS
	m_showResult = new QAction(tr("Show Result"), this);
    m_showResult->setIconText(tr("Result"));
	connect(m_showResult, &QAction::triggered, this, &MainWindow::onToggleResult);
	m_showResult->setToolTip(tr("Toggle Result (r)"));
        m_showResult->setCheckable(true);
	m_showResult->setChecked(true);
	m_showResult->setShortcut(QKeySequence("r"));
    m_showResult->setStatusTip(tr("Show or hide the processed removal result"));
    for (QAction *action :
         {m_polygon, m_mask, m_invert, m_detectCradle, m_removeCradle,
          m_removeTexture, m_showResult}) {
        addAction(action);
    }

    // INTENSITY
    {
		m_tonePanel = new QWidget;
        m_tonePanel->setObjectName("tonePanel");
		QHBoxLayout *layout = new QHBoxLayout(m_tonePanel);
		layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(6);

        QLabel *toneTitle = new QLabel(tr("Tone"), m_tonePanel);
        toneTitle->setObjectName("tonePanelTitle");
        toneTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(toneTitle);

        m_black = new Slider;
        m_black->setObjectName("black");
        m_black->setLabel(tr("Black"));
        m_black->setHelpText(tr("Black point"),
                             tr("Set the darkest output level for the selected removal segments."));
        m_black->setRange(0, 255);
        connect(m_black, &Slider::sliderPressed, this, &MainWindow::onBeginChange);
        connect(m_black, &Slider::sliderReleased, this, &MainWindow::onEndChange);
        connect(m_black, &Slider::valueChanged, this, &MainWindow::onSlider);

        m_gamma = new Slider;
        m_gamma->setObjectName("gamma");
        m_gamma->setLabel(tr("Gamma"));
        m_gamma->setHelpText(tr("Gamma"),
                             tr("Bias the midpoint brightness for the selected removal segments."));
        m_gamma->setRange(-100, 100);
        connect(m_gamma, &Slider::sliderPressed, this, &MainWindow::onBeginChange);
        connect(m_gamma, &Slider::sliderReleased, this, &MainWindow::onEndChange);
        connect(m_gamma, &Slider::valueChanged, this, &MainWindow::onSlider);

        m_white = new Slider;
        m_white->setObjectName("white");
        m_white->setLabel(tr("White"));
        m_white->setHelpText(tr("White point"),
                             tr("Set the brightest output level for the selected removal segments."));
        m_white->setRange(0, 255);
        connect(m_white, &Slider::sliderPressed, this, &MainWindow::onBeginChange);
        connect(m_white, &Slider::sliderReleased, this, &MainWindow::onEndChange);
        connect(m_white, &Slider::valueChanged, this, &MainWindow::onSlider);

		layout->addWidget(m_black);
		layout->addWidget(m_gamma);
		layout->addWidget(m_white);
        layout->addStretch(1);
        m_tonePanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }

	// TABS
	m_tabs = new QTabBar;
    m_tabs->setObjectName("modeTabs");
    m_tabs->setDocumentMode(true);
    m_tabs->setDrawBase(false);
    m_tabs->setFocusPolicy(Qt::NoFocus);
    m_tabs->setUsesScrollButtons(false);
	connect(m_tabs, &QTabBar::currentChanged, this, &MainWindow::onTab);
	m_tabs->blockSignals(true);
	m_tabs->setExpanding(false);
	m_tabs->addTab(tr("Mark"));
	m_tabs->addTab(tr("Remove"));
	m_tabs->addTab(tr("Texture"));
    m_tabs->setTabToolTip(kTab_Mark, tr("Mark cradle members and defect masks."));
    m_tabs->setTabToolTip(kTab_Remove, tr("Tune the removal result for selected output regions."));
    m_tabs->setTabToolTip(kTab_Texture, tr("Run texture removal after cradle cleanup is finished."));
	m_tabs->blockSignals(false);
    applyMacCompactSize(m_tabs);

    m_markControls = createActionRow("markControls",
                                     {m_polygon, m_mask, m_invert, m_detectCradle, m_removeCradle},
                                     this);
    m_removeControls = new QWidget(this);
    m_removeControls->setObjectName("removeControls");
    {
        QHBoxLayout *layout = new QHBoxLayout(m_removeControls);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(6);
        layout->addWidget(createActionButton(m_showResult, m_removeControls));
        layout->addWidget(m_tonePanel);
        layout->addStretch(1);
    }
    m_textureControls = createActionRow("textureControls", {m_removeTexture}, this);

    m_contextStack = new QStackedWidget(this);
    m_contextStack->setObjectName("contextStack");
    m_contextStack->addWidget(m_markControls);
    m_contextStack->addWidget(m_removeControls);
    m_contextStack->addWidget(m_textureControls);
    m_contextStack->setCurrentIndex(kTab_Mark);
    m_contextStack->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    m_modeStrip = new QWidget(this);
    m_modeStrip->setObjectName("modeStrip");
    m_modeStrip->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    {
        QHBoxLayout *layout = new QHBoxLayout(m_modeStrip);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(10);
        layout->addWidget(m_tabs, 0, Qt::AlignVCenter);
        layout->addWidget(m_contextStack, 0, Qt::AlignVCenter);
        layout->addStretch(1);
    }

	QWidget *widget = new QWidget;
    widget->setObjectName("mainWindowCentral");
	QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
	layout->addWidget(m_primaryBar);
	layout->addWidget(m_modeStrip);
	layout->addWidget(m_scroller);
	setCentralWidget(widget);

    m_primaryBar->setEnabled(false);
    m_modeStrip->setEnabled(false);

    m_progressBar = new QProgressBar;
    m_cancel = new QPushButton(tr("Cancel"));
    m_progressBar->setObjectName("statusProgress");
    m_progressBar->setFixedWidth(180);
    m_progressBar->setTextVisible(false);
    m_cancel->setObjectName("statusCancelButton");
    m_cancel->setFixedHeight(20);
    connect(m_cancel, &QPushButton::clicked, this, &MainWindow::onCancelProgress);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_cancel);
    m_progressBar->hide();
    m_cancel->hide();
	statusBar()->show();

	setupMenus();
    setWindowTitle(QCoreApplication::applicationName());
    refreshTheme();

    // set draw tool
    m_viewer->setTool(Viewer::kTool_Polygon);
}

MainWindow::~MainWindow()
{

}

void MainWindow::refreshTheme()
{
    if (m_refreshingTheme)
        return;

    m_refreshingTheme = true;
    const QString sheet = mainWindowStyleSheet(palette());
    if (styleSheet() != sheet)
        setStyleSheet(sheet);
    m_viewer->update();
    m_scroller->update();
    m_refreshingTheme = false;
}

void MainWindow::changeEvent(QEvent *event)
{
    if (!m_refreshingTheme &&
        (event->type() == QEvent::PaletteChange ||
         event->type() == QEvent::ApplicationPaletteChange ||
         event->type() == QEvent::StyleChange)) {
        refreshTheme();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::setupMenus()
{
	// file menu
	{
		QMenu *menu = menuBar()->addMenu(tr("&File"));

		// Open
		m_open = menu->addAction(tr("&Open Image...", "File|Open"));
		connect(m_open, &QAction::triggered, this, &MainWindow::onOpenImage);

		// Export
		m_export = menu->addAction(tr("&Export...", "File|Export"));
		connect(m_export, &QAction::triggered, this, &MainWindow::onExport);

		// Close
		m_close = menu->addAction(tr("&Close", "File|Close"));
		m_close->setShortcut(QKeySequence::Close);
		connect(m_close, &QAction::triggered, this, &MainWindow::onClose);

        // open cradle
		m_loadCradle = menu->addAction(tr("Open &Cradle...", "File|Open"));
		m_loadCradle->setShortcut(QKeySequence::Open);
		connect(m_loadCradle, &QAction::triggered, this, &MainWindow::onOpen);

		// Save
		m_save = menu->addAction(tr("&Save Cradle", "File|Save"));
		m_save->setShortcut(QKeySequence::Save);
		connect(m_save, &QAction::triggered, this, &MainWindow::onSave);

		// Save As
		m_saveAs = menu->addAction(tr("&Save Cradle As...", "File|Save As"));
		m_saveAs->setShortcut(QKeySequence::SaveAs);
		connect(m_saveAs, &QAction::triggered, this, &MainWindow::onSaveAs);

		menu->addSeparator();

		// Exit
		QAction *exit = menu->addAction(tr("E&xit"));
		exit->setShortcut(tr("Ctrl+Q", "File|Exit"));
		connect(exit, &QAction::triggered, this, &MainWindow::onExit);
	}

	// edit menu
	{
		m_editMenu = menuBar()->addMenu(tr("&Edit"));

		m_undo = m_editMenu->addAction(tr("&Undo", "Edit|Undo"));
		m_undo->setShortcut(QKeySequence::Undo);
		connect(m_undo, &QAction::triggered, m_undoMgr, &UndoManager::undo);

		m_redo = m_editMenu->addAction(tr("&Redo", "Edit|Redo"));
		m_redo->setShortcut(QKeySequence::Redo);
		connect(m_redo, &QAction::triggered, m_undoMgr, &UndoManager::redo);

		m_editMenu->addSeparator();

		// Cut
		{
			QAction *action = m_editMenu->addAction(tr("Cu&t", "Edit|Cut"));
			action->setShortcut(QKeySequence::Cut);
			action->setData("cut");
			connect(action, &QAction::triggered, this, &MainWindow::onCommand);
			m_commands << action;
		}

		// Copy
		{
			QAction *action = m_editMenu->addAction(tr("&Copy", "Edit|Copy"));
			action->setShortcut(QKeySequence::Copy);
			action->setData("copy");
			connect(action, &QAction::triggered, this, &MainWindow::onCommand);
			m_commands << action;
            m_copy = action;
		}

		// Paste
		{
			QAction *action = m_editMenu->addAction(tr("&Paste", "Edit|Paste"));
			action->setShortcut(QKeySequence::Paste);
			action->setData("paste");
			connect(action, &QAction::triggered, this, &MainWindow::onCommand);
			m_commands << action;
            m_paste = action;
		}

		// Delete
		{
			QAction *action = m_editMenu->addAction(tr("&Delete", "Edit|Delete"));
            #if defined(Q_OS_MACOS)
                action->setShortcut(QKeySequence(Qt::Key_Backspace));

                QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
                connect(shortcut, &QShortcut::activated, this, &MainWindow::onDelete);
            #else
                action->setShortcut(QKeySequence::Delete);
            #endif
			action->setData("delete");
			connect(action, &QAction::triggered, this, &MainWindow::onCommand);
			m_commands << action;
            m_delete = action;
		}

		m_editMenu->addSeparator();

		// Select All
		{
			QAction *action = m_editMenu->addAction(tr("&Select All", "Edit|Select All"));
			action->setShortcut(QKeySequence::SelectAll);
			action->setData("selectAll");
			connect(action, &QAction::triggered, this, &MainWindow::onCommand);
			m_commands << action;
		}
	}

	// view menu
	{
		m_viewMenu = menuBar()->addMenu(tr("&View"));

		QAction *zoomFit = m_viewMenu->addAction(tr("Zoom to &fit", "View|Zoom to Fit"));
		zoomFit->setShortcut(QKeySequence("f"));
		connect(zoomFit, &QAction::triggered, m_viewer, &Viewer::zoomToFit);

		QAction *zoom100 = m_viewMenu->addAction(tr("Zoom to 1:1", "View|Zoom to 1:1"));
		zoom100->setShortcut(QKeySequence("h"));
		connect(zoom100, &QAction::triggered, m_viewer, &Viewer::zoom100);

		QAction *zoomIn = m_viewMenu->addAction(tr("Zoom &in", "View|Zoom In"));
		zoomIn->setShortcut(QKeySequence("i"));
		connect(zoomIn, &QAction::triggered, m_viewer, &Viewer::zoomIn);

		QAction *zoomOut = m_viewMenu->addAction(tr("Zoom &out", "View|Zoom Out"));
		zoomOut->setShortcut(QKeySequence("o"));
		connect(zoomOut, &QAction::triggered, m_viewer, &Viewer::zoomOut);
	}

	// help menu
	{
		QMenu *menu = menuBar()->addMenu(tr("&Help"));

		QAction *helpAction = menu->addAction(tr("&User Guide...", "Help|User Guide"));
		helpAction->setShortcut(QKeySequence("F1"));
		connect(helpAction, &QAction::triggered, this, &MainWindow::onHelp);

		QAction *aboutAction = menu->addAction(tr("&About...", "Help|About"));
		connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
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
	m_primaryBar->setEnabled(true);
    m_modeStrip->setEnabled(true);
	onTab(m_tabs->currentIndex());
}

void MainWindow::open(const QString &path, bool pluginMode)
{
	onClose();

    m_pendingImagePath = path;
    m_pendingPluginMode = pluginMode;
	ImageManager::get().load(path);
}

//
// SLOTS
//
void MainWindow::onOpenImage()
{
    pp("onOpenImage", __LINE__);
	QSettings settings;
	QString dir = settings.value("/imageDir").toString();
	QString filter = tr("Project Files (*.%1);;Image Files (*.tif *.tiff *.png);;All Files (*)").arg(kProjectExtension);
    QFileDialog dialog(this, tr("Open Image"), dir, filter);
    dialog.setFileMode(QFileDialog::ExistingFile);
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
    pp("onOpen", __LINE__);
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
    m_pendingImagePath.clear();
    m_pendingPluginMode = false;
    m_pluginMode = false;
	m_viewer->setTool(Viewer::kTool_None);
	m_primaryBar->setEnabled(false);
    m_modeStrip->setEnabled(false);

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
        if (!m_pendingImagePath.isEmpty())
        {
            m_pluginMode = m_pendingPluginMode;
            m_viewer->setTool(Viewer::kTool_Polygon);
            loadCradle(QString());
            m_project->setImagePath(m_pendingImagePath);
            m_open->setVisible(!m_pluginMode);
            m_export->setVisible(!m_pluginMode);
            m_close->setVisible(!m_pluginMode);
            m_pluginControls->setVisible(m_pluginMode);
            m_pendingImagePath.clear();
        }
        statusBar()->showMessage(QString("Image Size: %1x%2").arg(source->size().width()).arg(source->size().height()));
    }
}

void MainWindow::onImageLoadFailed(const QString &message)
{
    if (m_pendingImagePath.isEmpty())
        return;

    m_pendingImagePath.clear();
    m_pendingPluginMode = false;
    m_pluginMode = false;
    m_open->setVisible(true);
    m_export->setVisible(true);
    m_close->setVisible(true);
    m_pluginControls->setVisible(false);
    statusBar()->clearMessage();
    QMessageBox::critical(this, QCoreApplication::applicationName(), message);
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
    m_contextStack->setCurrentIndex(index);

    m_tonePanel->setVisible(index == kTab_Remove);

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
        return cv::cvarrToMat(imageref.get());
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
              return cv::cvarrToMat(imageref.get());
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
				QMessageBox::Ok);
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
              return cv::cvarrToMat(imageref.get());
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
              return cv::cvarrToMat(imageref.get());
            };
    cvImageRef mask = buildMask(m_project);
    cv::Mat maskMat(arr_to_mat(mask));

    cv::Mat outMat;

    CradleCallbacks progress(this);
    CradleFunctions::setCallbacks(&progress);

    beginProgress(tr("Removing Texture..."));

    try
    {
        TextureRemoval::Status status = TextureRemoval::textureRemove(
            resultMat, maskMat, outMat, Project::activeProject()->markedSegments());
        if (status != TextureRemoval::Status::kInsufficientSamples)
            outMat.copyTo(resultMat);

        CradleFunctions::setCallbacks(nullptr);
        endProgress();

        TextureRemovalMessage message = textureRemovalMessage(status);
        if (message.shouldDisplay()) {
            QMessageBox box(message.icon, message.title, message.body,
                            QMessageBox::Ok, this);
            box.exec();
        }
    }
    catch (const std::exception &e)
    {
        CradleFunctions::setCallbacks(nullptr);
        endProgress();
        QMessageBox::critical(this, QCoreApplication::applicationName(), e.what());
        return;
    }

    source->invalidate();
}

void MainWindow::onHelp()
{
    QString helpPath;
#if defined(Q_OS_MACOS)
    QDir appDir(QCoreApplication::applicationDirPath());
    helpPath = QDir(appDir.absoluteFilePath("../Resources"))
                   .absoluteFilePath("user_guide.pdf");
#else
    helpPath = QDir(QCoreApplication::applicationDirPath())
                   .absoluteFilePath("user_guide.pdf");
#endif
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
