#include "test_helpers.h"

#include <gtest/gtest.h>

#include <platypus/CradleFunctions.h>
#include <platypus/TextureRemoval.h>

#include <filesystem>

namespace {

void ExpectFiniteMat(const cv::Mat& image) {
  EXPECT_TRUE(test_helpers::isFinite(image));
}

void ExpectValidRanges(const std::vector<int>& ranges, int upper_bound) {
  ASSERT_EQ(ranges.size() % 2, 0u);
  for (size_t i = 0; i < ranges.size(); i += 2) {
    EXPECT_GE(ranges[i], 0);
    EXPECT_GE(ranges[i + 1], ranges[i]);
    EXPECT_LE(ranges[i + 1], upper_bound);
  }
}

cv::Mat MakeSyntheticTextureImage(int rows = 64, int cols = 64) {
  cv::Mat image(rows, cols, CV_32F);
  cv::randu(image, 0.0f, 255.0f);
  return image;
}

CradleFunctions::MarkedSegments MakeSegmentLayout(const cv::Size& size) {
  CradleFunctions::MarkedSegments segments;
  segments.pieces = 2;
  segments.piece_mask = cv::Mat(size, CV_16U, cv::Scalar(0));
  segments.piece_middle.push_back(cv::Point2i(size.width / 2, size.height / 2));
  segments.piece_type.push_back(CradleFunctions::VERTICAL_DIR);
  segments.pieceIDh = {};
  segments.pieceIDv = {{1}};
  return segments;
}

}  // namespace

TEST(PlatypusBackend, CradleDetectFindsMembersOnFixture) {
  cv::Mat image = test_helpers::loadFixtureGrayscaleFloat("cradle.jpg");
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  std::vector<int> vrange;
  std::vector<int> hrange;

  CradleFunctions::cradledetect(image, mask, vrange, hrange);

  EXPECT_FALSE(vrange.empty() && hrange.empty());
  ExpectValidRanges(vrange, image.cols);
  ExpectValidRanges(hrange, image.rows);
}

TEST(PlatypusBackend, RemoveCradleProducesFiniteOutputsAndSegments) {
  cv::Mat image = test_helpers::loadFixtureGrayscaleFloat("cradle.jpg");
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  cv::Mat out;
  cv::Mat cradle;
  CradleFunctions::MarkedSegments segments;

  CradleFunctions::removeCradle(image, out, cradle, mask, segments);

  ASSERT_EQ(out.size(), image.size());
  ASSERT_EQ(cradle.size(), image.size());
  ASSERT_EQ(segments.piece_mask.size(), image.size());
  EXPECT_GT(segments.pieces, 0);
  ExpectFiniteMat(out);
  ExpectFiniteMat(cradle);
  EXPECT_FALSE(segments.pieceIDh.empty() && segments.pieceIDv.empty());
}

TEST(PlatypusBackend, MarkedSegmentsRoundTripPersistsMetadata) {
  cv::Mat image = test_helpers::loadFixtureGrayscaleFloat("cradle.jpg");
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  cv::Mat out;
  cv::Mat cradle;
  CradleFunctions::MarkedSegments written;

  CradleFunctions::removeCradle(image, out, cradle, mask, written);

  std::string path = test_helpers::temporaryPath("platypus-segments", ".mask");
  CradleFunctions::writeMarkedSegmentsFile(path, written);
  CradleFunctions::MarkedSegments loaded =
      CradleFunctions::readMarkedSegmentsFile(path);
  std::filesystem::remove(path);

  EXPECT_EQ(loaded.pieces, written.pieces);
  EXPECT_EQ(loaded.pieceIDh, written.pieceIDh);
  EXPECT_EQ(loaded.pieceIDv, written.pieceIDv);
  EXPECT_EQ(loaded.piece_type, written.piece_type);
  EXPECT_EQ(loaded.piece_middle.size(), written.piece_middle.size());
  EXPECT_EQ(loaded.piece_mask.size(), written.piece_mask.size());
}

TEST(PlatypusBackend, TextureRemovalPipelineCompletesOnFixture) {
  cv::Mat image = test_helpers::loadFixtureGrayscaleFloat("cradle.jpg");
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  cv::Mat cradle_removed;
  cv::Mat cradle;
  CradleFunctions::MarkedSegments segments;

  CradleFunctions::removeCradle(image, cradle_removed, cradle, mask, segments);

  cv::Mat texture_removed;
  TextureRemoval::Status status =
      TextureRemoval::textureRemove(cradle_removed, mask, texture_removed, segments);

  EXPECT_NE(status, TextureRemoval::Status::kInsufficientSamples);
  ASSERT_EQ(texture_removed.size(), image.size());
  ExpectFiniteMat(texture_removed);
  EXPECT_GT(cv::norm(texture_removed, cradle_removed, cv::NORM_L1), 0.0);
}

TEST(PlatypusBackend, TextureRemovalFallsBackWhenLocalClustersAreUnavailable) {
  cv::Mat image = MakeSyntheticTextureImage();
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  cv::Mat out;
  CradleFunctions::MarkedSegments segments = MakeSegmentLayout(image.size());

  for (int row = 0; row < image.rows; ++row) {
    segments.piece_mask.at<unsigned short>(row, 4) = 1;
  }

  TextureRemoval::Status status =
      TextureRemoval::textureRemove(image, mask, out, segments);

  EXPECT_EQ(status, TextureRemoval::Status::kFallbackModel);
  ASSERT_EQ(out.size(), image.size());
  ExpectFiniteMat(out);
}

TEST(PlatypusBackend, TextureRemovalUsesLimitedLocalSamplesWhenClustersAreSparse) {
  cv::Mat image = MakeSyntheticTextureImage(256, 256);
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  cv::Mat out;
  CradleFunctions::MarkedSegments segments = MakeSegmentLayout(image.size());

  for (int row = 48; row <= 66; ++row) {
    segments.piece_mask.at<unsigned short>(row, 48) = 1;
  }

  TextureRemoval::Status status =
      TextureRemoval::textureRemove(image, mask, out, segments);

  EXPECT_EQ(status, TextureRemoval::Status::kLimitedLocalSamples);
  ASSERT_EQ(out.size(), image.size());
  ExpectFiniteMat(out);
}

TEST(PlatypusBackend, TextureRemovalFailsCleanlyWithoutUsableNonCradleSamples) {
  cv::Mat image = MakeSyntheticTextureImage();
  cv::Mat mask = test_helpers::makeEmptyMask(image);
  cv::Mat out;
  CradleFunctions::MarkedSegments segments = MakeSegmentLayout(image.size());
  segments.piece_mask.setTo(cv::Scalar(1));

  TextureRemoval::Status status =
      TextureRemoval::textureRemove(image, mask, out, segments);

  EXPECT_EQ(status, TextureRemoval::Status::kInsufficientSamples);
  EXPECT_TRUE(out.empty());
}
