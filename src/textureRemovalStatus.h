#ifndef TEXTUREREMOVALSTATUS_H
#define TEXTUREREMOVALSTATUS_H

#include <platypus/TextureRemoval.h>
#include <QtWidgets/QMessageBox>
#include <QtCore/QString>

struct TextureRemovalMessage {
  QMessageBox::Icon icon;
  QString title;
  QString body;

  bool shouldDisplay() const { return icon != QMessageBox::NoIcon; }
};

TextureRemovalMessage textureRemovalMessage(TextureRemoval::Status status);

#endif
