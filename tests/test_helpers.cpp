#include "test_helpers.h"

#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <limits>
#include <random>
#include <stdexcept>

namespace test_helpers {
namespace fs = std::filesystem;

std::string fixturePath(const std::string& name) {
  return (fs::path(PLATYPUS_TEST_IMAGE_DIR) / name).string();
}

cv::Mat loadFixtureGrayscaleFloat(const std::string& name) {
  cv::Mat image = cv::imread(fixturePath(name), cv::IMREAD_GRAYSCALE);
  if (image.empty()) {
    throw std::runtime_error("Failed to load fixture image: " + name);
  }

  cv::Mat result;
  image.convertTo(result, CV_32F);
  return result;
}

cv::Mat makeEmptyMask(const cv::Mat& image) {
  return cv::Mat(image.rows, image.cols, CV_8U, cv::Scalar(0));
}

bool isFinite(const cv::Mat& image) {
  return cv::checkRange(image, true, nullptr, -std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity());
}

std::string temporaryPath(const std::string& stem, const std::string& extension) {
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, 999999);
  fs::path path = fs::temp_directory_path() /
                  (stem + "-" + std::to_string(dist(rd)) + extension);
  return path.string();
}

}  // namespace test_helpers
