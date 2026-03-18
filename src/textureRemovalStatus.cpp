#include <textureRemovalStatus.h>

#include <QObject>

TextureRemovalMessage textureRemovalMessage(TextureRemoval::Status status) {
  switch (status) {
    case TextureRemoval::Status::kSuccess:
      return {QMessageBox::NoIcon, QString(), QString()};
    case TextureRemoval::Status::kLimitedLocalSamples:
      return {
          QMessageBox::Warning,
          QObject::tr("Texture Removal Completed with Limited Local Samples"),
          QObject::tr("Platypus found fewer local texture samples than expected in part of the image and used the available samples to continue. Texture removal completed, but some areas may be less smooth or less accurate than usual.")};
    case TextureRemoval::Status::kFallbackModel:
      return {
          QMessageBox::Warning,
          QObject::tr("Texture Removal Used Fallback Modeling"),
          QObject::tr("Platypus could not find enough local texture examples in part of the image, so it used a broader fallback model instead. Texture removal completed, but those regions may need closer review.")};
    case TextureRemoval::Status::kInsufficientSamples:
      return {
          QMessageBox::Critical,
          QObject::tr("Texture Removal Could Not Continue"),
          QObject::tr("Platypus could not find enough usable texture samples to build a reliable texture model for this image, so texture removal was stopped and no texture changes were applied.\n\nTo improve this:\n1. Run cradle removal first if it has not been completed yet.\n2. Reduce the defect mask so more undamaged wood texture remains available for sampling.\n3. Adjust the cradle selection so the lattice is marked accurately.\n4. Retry from the original image or from the cradle-removal result before further edits.")};
  }

  return {QMessageBox::NoIcon, QString(), QString()};
}
