#include "test_helpers.h"

#include <gtest/gtest.h>

#include <imageManager.h>
#include <mainWindow.h>
#include <polygonCommands.h>
#include <project.h>
#include <textureRemovalStatus.h>
#include <undoManager.h>
#include <viewer.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMetaObject>
#include <QtCore/QTemporaryDir>

#include <functional>

namespace {

bool WaitForCondition(const std::function<bool()>& condition, int timeout_ms = 10000) {
  QElapsedTimer timer;
  timer.start();
  while (!condition()) {
    if (timer.elapsed() > timeout_ms)
      return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  return true;
}

class FakeImageSource : public ImageSource {
 public:
  explicit FakeImageSource(QObject* parent = nullptr) : ImageSource(parent) {
    image_ = QImage(64, 64, QImage::Format_Indexed8);
    image_.fill(0);
    image_.setColorCount(256);
    for (int i = 0; i < 256; ++i)
      image_.setColor(i, qRgba(i, i, i, 255));
  }

  QSize size() const override { return image_.size(); }
  QString name() const override { return "Fake"; }
  bool isValid() const override { return true; }

  QImage getTile(const QRect& tile, const QSize& size) const override {
    return image_.copy(tile).scaled(size);
  }

 private:
  QImage image_;
};

}  // namespace

TEST(PlatypusQt, TextureRemovalMessagesMatchStatus) {
  TextureRemovalMessage limited =
      textureRemovalMessage(TextureRemoval::Status::kLimitedLocalSamples);
  EXPECT_EQ(limited.icon, QMessageBox::Warning);
  EXPECT_TRUE(limited.title.contains("Limited Local Samples"));

  TextureRemovalMessage fallback =
      textureRemovalMessage(TextureRemoval::Status::kFallbackModel);
  EXPECT_EQ(fallback.icon, QMessageBox::Warning);
  EXPECT_TRUE(fallback.title.contains("Fallback Modeling"));

  TextureRemovalMessage failure =
      textureRemovalMessage(TextureRemoval::Status::kInsufficientSamples);
  EXPECT_EQ(failure.icon, QMessageBox::Critical);
  EXPECT_TRUE(failure.body.contains("Run cradle removal first"));
}

TEST(PlatypusQt, ProjectRoundTripPreservesImagePathAndPolygons) {
  Project project;
  project.setImagePath("fixture.tif");
  PolygonPointer poly(new Polygon(Polygon::INPUT));
  poly->set(QRect(10, 20, 30, 40));
  project.addPolygon(poly);

  QByteArray saved = project.save();

  Project loaded;
  loaded.load(saved);

  EXPECT_EQ(loaded.imagePath(), QString("fixture.tif"));
  ASSERT_EQ(loaded.polygons().size(), 1);
  EXPECT_EQ(loaded.polygons()[0]->type(), Polygon::INPUT);
}

TEST(PlatypusQt, UndoManagerCommandsUndoAndRedoPolygonEdits) {
  Project project;
  UndoManager undo;
  PolygonPointer poly(new Polygon(Polygon::INPUT));
  poly->set(QRect(0, 0, 10, 20));
  const QRect original_rect = poly->boundingRect();

  undo.push(new AddPolygonCommand(poly));
  ASSERT_EQ(project.polygons().size(), 1);

  Polygon updated(Polygon::INPUT);
  updated.set(QRect(5, 5, 15, 25));
  const QRect updated_rect = updated.boundingRect();
  undo.push(new EditPolygonCommand(poly, updated.points()));
  EXPECT_EQ(project.polygons()[0]->boundingRect(), updated_rect);

  undo.undo();
  EXPECT_EQ(project.polygons()[0]->boundingRect(), original_rect);

  undo.redo();
  EXPECT_EQ(project.polygons()[0]->boundingRect(), updated_rect);
}

TEST(PlatypusQt, ImageManagerLoadsAndClearsFixture) {
  ImageManager manager;

  manager.load(QString::fromStdString(test_helpers::fixturePath("cradle.jpg")));
  ASSERT_TRUE(WaitForCondition([&manager] { return manager.source() != nullptr; }));

  EXPECT_NE(manager.source(), nullptr);
  EXPECT_NE(manager.floatImage().get(), nullptr);
  EXPECT_NE(manager.resultImage().get(), nullptr);
  EXPECT_NE(manager.removeMask().get(), nullptr);

  manager.clear();
  EXPECT_EQ(manager.source(), nullptr);
}

TEST(PlatypusQt, ViewerSmokeWorksOffscreen) {
  ImageManager manager;
  Project project;
  Viewer viewer;
  FakeImageSource source;

  viewer.resize(320, 240);
  viewer.setSource(&source);
  viewer.zoomToFit();
  viewer.zoomIn();
  viewer.zoomOut();
  viewer.scroll(5, 5);

  EXPECT_EQ(viewer.getSource(), &source);
  EXPECT_FALSE(viewer.getVisibleDestRect().isNull());
  EXPECT_FALSE(viewer.getDestRect().isNull());
}

TEST(PlatypusQt, MainWindowOpenAndCloseSmokeWorks) {
  ImageManager manager;
  Project project;
  MainWindow window(&project);

  window.show();
  window.open(QString::fromStdString(test_helpers::fixturePath("cradle.jpg")));
  ASSERT_TRUE(WaitForCondition([&manager] { return manager.source() != nullptr; }));

  EXPECT_NE(manager.source(), nullptr);
  EXPECT_FALSE(project.imagePath().isEmpty());

  QMetaObject::invokeMethod(&window, "onClose", Qt::DirectConnection);
  EXPECT_EQ(manager.source(), nullptr);
}
