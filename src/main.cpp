#include <memory_resource>
#include <mainWindow.h>
#include <imageManager.h>
#include <project.h>
#include <QtWidgets/QApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>

bool s_debug_mode = true;

int main(int argc, char *argv[])
{
	QCoreApplication::setApplicationName("Platypus");
	QCoreApplication::setApplicationVersion("1.0");
	QCoreApplication::setOrganizationName("Digital Film Tools");
	QCoreApplication::setOrganizationDomain("com.digitalfilmtools");
	{
        #if defined(Q_OS_MACX)
            QString pluginsPath = QFileInfo(argv[0]).path() + "/../Resources/plugins";
        #else
            QString pluginsPath = QFileInfo(argv[0]).path() + "/plugins";
        #endif
		QCoreApplication::setLibraryPaths(QStringList() << pluginsPath);
	}

	QApplication a(argc, argv);
    a.setAttribute(Qt::AA_UseHighDpiPixmaps);

    QSettings settings;
	ImageManager imageManager;
	Project project;

	MainWindow w(&project);

    QByteArray state = settings.value("/window/state").toByteArray();
    w.restoreState(state);
    if (settings.value("/window/maximized", true).toBool())
        w.showMaximized();
    else
    {
        QRect geometry = settings.value("/window/geometry", QRect()).toRect();
        if (geometry.isValid())
            w.setGeometry(geometry);
        w.show();
    }

	// parse arguments
    bool standalone = false;
	QStringList args = a.arguments();
	QString path;
	for (int i = 1; i < args.size(); i++)
	{
        if (args[i] == "-image" && i < args.size() - 2)
            path = args[++i];
        else if (args[i] == "-project" && i < args.size() - 2)
            path = args[++i];
        else if (args[i] == "-debug")
            s_debug_mode = true;
        else if (args[i] == "-standalone")
            standalone = true;
	}
    if (path.isEmpty() && args.size() > 1 && !standalone)
	{
		if (!args.back().startsWith("-"))
			path = args.back();
	}

	if (!path.isEmpty())
	{
		w.open(path, true);
	}

	int result =  a.exec();

    settings.setValue("/window/geometry", w.geometry());
    settings.setValue("/window/state", w.saveState());
    settings.setValue("/window/maximized", w.isMaximized());

    return result;
}
