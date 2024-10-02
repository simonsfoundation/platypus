#include "tiff.h"
#include <libtiff/tiffio.h>

static void s_errorHandler(const char*, const char*, va_list)
{
	int tmp = 0;
}

TIFFImage::TIFFImage() : m_tiff(NULL)
{
	::TIFFSetErrorHandler(s_errorHandler);
	::TIFFSetWarningHandler(s_errorHandler);
}

TIFFImage::~TIFFImage()
{
}

void TIFFImage::close()
{
	if (m_tiff)
	{
		::TIFFClose(m_tiff);
		m_tiff = NULL;
	}
}

bool TIFFImage::create(const char *path, int width, int height)
{
	TIFF *tiff = ::TIFFOpen(path, "w");
	if (tiff)
	{
		m_width = width;
		m_height = height;
		m_depth = 8;

		int n_channels = 1;
		int bpc = m_depth / 8;

		::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, (uint32)width);
		::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, (uint32)height);
		::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, m_depth);
		::TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
		::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
		::TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, n_channels);
		::TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 1);
		::TIFFSetField(tiff, TIFFTAG_MINSAMPLEVALUE, (uint16)0);
        ::TIFFSetField(tiff, TIFFTAG_MAXSAMPLEVALUE, (uint16)255);
	}
	m_tiff = tiff;

	return m_tiff != NULL;
}

bool TIFFImage::write(const char *ptr, int row, int rows, int rowBytes)
{
	for (int i = 0; i < rows; i++)
	{
		if (::TIFFWriteScanline(m_tiff, (void *)ptr, row + i) != 1)
			return false;

		ptr += rowBytes;
	}

	return true;
}

bool TIFFImage::open(const char *path)
{
	TIFF *tiff = ::TIFFOpen(path, "r");
	if (tiff)
	{
		uint32 width, height;
		uint16 chan_bits, sample_format;
		uint16 orientation = ORIENTATION_TOPLEFT;
		::TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
		::TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
		::TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &chan_bits);
		::TIFFGetField(tiff, TIFFTAG_ORIENTATION, &orientation);
		::TIFFGetField(tiff, TIFFTAG_SAMPLEFORMAT, &sample_format);
		uint16 pixel_samples;
		::TIFFGetField(tiff, TIFFTAG_SAMPLESPERPIXEL, &pixel_samples);

		m_width = int(width);
		m_height = int(height);
		m_depth = int(chan_bits);
	}
	m_tiff = tiff;

	return m_tiff != NULL;
}

bool TIFFImage::read(char *ptr, int row, int rows, int rowBytes)
{
	for (int i = 0; i < rows; i++)
	{
		if (::TIFFReadScanline(m_tiff, (void *)ptr, row + i, 0) != 1)
			return false;

		ptr += rowBytes;
	}

	return true;
}

