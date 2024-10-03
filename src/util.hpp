#pragma once
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <QImage>

template <typename T>
void debug_fun(const T& x, int line_num) {
    qDebug() << "Line: " << line_num << " " << x;
}

template <typename T>
void pp(const T& x, int line_num) {
    qDebug() << "Line: " << line_num << " " << x;
}

