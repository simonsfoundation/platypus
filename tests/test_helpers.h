#ifndef PLATYPUS_TEST_HELPERS_H
#define PLATYPUS_TEST_HELPERS_H

#include <opencv2/core.hpp>
#include <string>

namespace test_helpers {

std::string fixturePath(const std::string& name);
cv::Mat loadFixtureGrayscaleFloat(const std::string& name);
cv::Mat makeEmptyMask(const cv::Mat& image);
bool isFinite(const cv::Mat& image);
std::string temporaryPath(const std::string& stem, const std::string& extension);

}  // namespace test_helpers

#endif
