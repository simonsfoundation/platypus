#ifndef Tiff_H
#define Tiff_H

#include <vector>

typedef struct tiff TIFF;

class TIFFImage
{
	TIFF *m_tiff;
	int m_scanLine;
	int m_depth;
	int m_width;
	int m_height;

public:
	TIFFImage();
	~TIFFImage();

	void close();
	int width() const { return m_width; }
	int height() const { return m_height; }
	int depth() const { return m_depth; }

	bool create(const char *path, int width, int height);
	bool write(const char *ptr, int row, int rows, int rowBytes);

	bool open(const char *path);
	bool read(char *ptr, int row, int rows, int rowBytes);
};

#endif
