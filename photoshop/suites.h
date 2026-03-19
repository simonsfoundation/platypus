#ifndef PLATYPUS_PHOTOSHOP_SUITES_H
#define PLATYPUS_PHOTOSHOP_SUITES_H

#include "SPBasic.h"

extern SPBasicSuite* sSPBasic;

template <typename T>
class SuiteHandler {
 public:
  SuiteHandler(SPBasicSuite* basic,
               const char* suite_name,
               int32 suite_version)
      : basic_(basic), suite_name_(suite_name), suite_version_(suite_version) {
    if (basic_ == nullptr) {
      throw kSPBadParameterError;
    }

    const void* acquired_suite = nullptr;
    SPErr err =
        basic_->AcquireSuite(suite_name_, suite_version_, &acquired_suite);
    if (err != kSPNoError) {
      suite_ = nullptr;
      throw err;
    }
    suite_ = const_cast<T*>(static_cast<const T*>(acquired_suite));
  }

  ~SuiteHandler() {
    if (suite_ != nullptr && basic_ != nullptr) {
      basic_->ReleaseSuite(suite_name_, suite_version_);
    }
  }

  const T* operator->() const { return suite_; }
  const T* get() const { return suite_; }

 private:
  SPBasicSuite* basic_;
  const char* suite_name_;
  int32 suite_version_;
  T* suite_ = nullptr;
};

#endif
