#!/bin/bash
APP_PATH="build/PlatypusGui.app"
OPENCV_LIB_PATH="/usr/local/opt/opencv@4/lib"

# Copy OpenCV libraries
mkdir -p "$APP_PATH/Contents/Frameworks"
cp $OPENCV_LIB_PATH/*.dylib "$APP_PATH/Contents/Frameworks/"

# Fix library install names
for dylib in "$APP_PATH"/Contents/Frameworks/*.dylib; do
  install_name_tool -id @executable_path/../Frameworks/$(basename "$dylib") "$dylib"
done

# Fix executable dependencies
install_name_tool -add_rpath @executable_path/../Frameworks "$APP_PATH/Contents/MacOS/PlatypusGui"
