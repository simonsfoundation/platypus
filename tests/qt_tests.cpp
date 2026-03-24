#include "test_helpers.h"

#include <gtest/gtest.h>

#include <dicomLoader.h>
#include <imageManager.h>
#include <mainWindow.h>
#include <polygonCommands.h>
#include <project.h>
#include <textureRemovalStatus.h>
#include <undoManager.h>
#include <viewer.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QMetaObject>
#include <QtCore/QTemporaryDir>

#include <cstdint>
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

struct LoadOutcome {
  bool success = false;
  QString error;
};

LoadOutcome WaitForLoad(ImageManager& manager, int timeout_ms = 10000) {
  LoadOutcome outcome;
  QObject::connect(&manager, &ImageManager::loadFailed, &manager,
                   [&](const QString& error) { outcome.error = error; },
                   Qt::SingleShotConnection);

  const bool finished = WaitForCondition(
      [&] { return manager.source() != nullptr || !outcome.error.isEmpty(); }, timeout_ms);
  EXPECT_TRUE(finished);
  outcome.success = manager.source() != nullptr;
  return outcome;
}

std::string writeTempTiff(const cv::Mat& image, const std::string& stem) {
  const std::string path = test_helpers::temporaryPath(stem, ".tiff");
  const bool ok = cv::imwrite(path, image);
  EXPECT_TRUE(ok);
  return path;
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

TEST(PlatypusQt, ImageManagerLoadsExistingTiffFixture) {
  ImageManager manager;

  manager.load(QString::fromStdString(test_helpers::fixturePath("test.platypus.mask.tiff")));
  const LoadOutcome outcome = WaitForLoad(manager);

  ASSERT_TRUE(outcome.success);
  ASSERT_NE(manager.source(), nullptr);
  EXPECT_EQ(manager.source()->size(), QSize(2000, 1568));
}

TEST(PlatypusQt, ImageManagerLoadsSixteenBitGrayscaleTiff) {
  ImageManager manager;
  cv::Mat image(48, 64, CV_16U);
  for (int y = 0; y < image.rows; ++y) {
    for (int x = 0; x < image.cols; ++x)
      image.at<std::uint16_t>(y, x) = std::uint16_t(1000 + (y * image.cols + x) * 8);
  }

  const std::string path = writeTempTiff(image, "platypus-gray16");
  manager.load(QString::fromStdString(path));
  const LoadOutcome outcome = WaitForLoad(manager);

  ASSERT_TRUE(outcome.success);
  ASSERT_NE(manager.source(), nullptr);
  const QImage tile =
      manager.source()->getTile(QRect(QPoint(0, 0), manager.source()->size()), manager.source()->size());
  ASSERT_FALSE(tile.isNull());
  EXPECT_EQ(tile.format(), QImage::Format_Indexed8);
  EXPECT_NE(tile.pixelIndex(0, 0), tile.pixelIndex(tile.width() - 1, tile.height() - 1));
}

TEST(PlatypusQt, ImageManagerLoadsColorTiffAsGrayscale) {
  ImageManager manager;
  cv::Mat image(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
  for (int y = 0; y < image.rows; ++y) {
    for (int x = 0; x < image.cols; ++x)
      image.at<cv::Vec3b>(y, x) = cv::Vec3b(std::uint8_t(x * 4), std::uint8_t(y * 6),
                                            std::uint8_t((x + y) * 3));
  }

  const std::string path = writeTempTiff(image, "platypus-color");
  manager.load(QString::fromStdString(path));
  const LoadOutcome outcome = WaitForLoad(manager);

  ASSERT_TRUE(outcome.success);
  ASSERT_NE(manager.source(), nullptr);
  const QImage tile =
      manager.source()->getTile(QRect(QPoint(0, 0), manager.source()->size()), manager.source()->size());
  ASSERT_FALSE(tile.isNull());
  EXPECT_EQ(tile.format(), QImage::Format_Indexed8);
  EXPECT_NE(tile.pixelIndex(0, 0), tile.pixelIndex(tile.width() - 1, tile.height() - 1));
}

TEST(PlatypusQt, ImageManagerFailsCleanlyForCorruptTiff) {
  ImageManager manager;
  const std::string path = test_helpers::temporaryPath("platypus-corrupt", ".tiff");

  QFile file(QString::fromStdString(path));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write("not-a-real-tiff");
  file.close();

  manager.load(QString::fromStdString(path));
  const LoadOutcome outcome = WaitForLoad(manager);

  EXPECT_FALSE(outcome.success);
  EXPECT_EQ(manager.source(), nullptr);
  EXPECT_FALSE(outcome.error.isEmpty());
}

TEST(PlatypusQt, DicomLoaderInspectsSingleFileFixture) {
  const QString path =
      QString::fromStdString(test_helpers::fixturePath("Anonymized_20260324.dcm"));

  const DicomFileInfo info = inspectDicomFile(path);
  ASSERT_TRUE(info.isDicom);
  EXPECT_TRUE(info.errorString.isEmpty());
  ASSERT_EQ(info.slices.size(), 1);

  const DicomRenderResult rendered = renderDicomSlice(info.slices.first());
  ASSERT_TRUE(rendered.errorString.isEmpty());
  ASSERT_FALSE(rendered.image.isNull());
  EXPECT_EQ(rendered.image.size(), QSize(256, 256));
  EXPECT_EQ(rendered.image.format(), QImage::Format_Indexed8);
}

TEST(PlatypusQt, ImageManagerLoadsRenderedDicomSlice) {
  ImageManager manager;
  const QString path =
      QString::fromStdString(test_helpers::fixturePath("Anonymized_20260324.dcm"));

  const DicomFileInfo info = inspectDicomFile(path);
  ASSERT_TRUE(info.isDicom);
  ASSERT_TRUE(info.errorString.isEmpty());
  ASSERT_EQ(info.slices.size(), 1);

  const DicomRenderResult rendered = renderDicomSlice(info.slices.first());
  ASSERT_TRUE(rendered.errorString.isEmpty());
  ASSERT_FALSE(rendered.image.isNull());

  manager.load(QStringLiteral("Anonymized_20260324.dcm"), rendered.image);
  const LoadOutcome outcome = WaitForLoad(manager);

  ASSERT_TRUE(outcome.success);
  ASSERT_NE(manager.source(), nullptr);
  EXPECT_EQ(manager.source()->size(), QSize(256, 256));
}

TEST(PlatypusQt, DicomLoaderInspectsTopLevelSeriesDirectory) {
  const QString directoryPath =
      QString::fromStdString(test_helpers::fixturePath("series-00000"));

  const DicomDirectoryInfo info = inspectDicomDirectory(directoryPath);
  EXPECT_TRUE(info.errorString.isEmpty());
  ASSERT_EQ(info.series.size(), 1);
  ASSERT_EQ(info.series.first().slices.size(), 27);

  const int middleSlice = info.series.first().slices.size() / 2;
  const DicomRenderResult rendered =
      renderDicomSlice(info.series.first().slices.at(middleSlice));
  ASSERT_TRUE(rendered.errorString.isEmpty());
  ASSERT_FALSE(rendered.image.isNull());
  EXPECT_EQ(rendered.image.size(), QSize(256, 256));
}

TEST(PlatypusQt, DicomDirectoryScanIgnoresNestedSubdirectories) {
  QTemporaryDir tempDir;
  ASSERT_TRUE(tempDir.isValid());

  const QString nestedDir = tempDir.filePath("nested");
  ASSERT_TRUE(QDir().mkpath(nestedDir));

  const QString sourcePath =
      QString::fromStdString(test_helpers::fixturePath("Anonymized_20260324.dcm"));
  const QString nestedPath = QDir(nestedDir).filePath("Anonymized_20260324.dcm");
  ASSERT_TRUE(QFile::copy(sourcePath, nestedPath));

  const DicomDirectoryInfo info = inspectDicomDirectory(tempDir.path());
  EXPECT_TRUE(info.series.isEmpty());
  EXPECT_FALSE(info.errorString.isEmpty());
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
