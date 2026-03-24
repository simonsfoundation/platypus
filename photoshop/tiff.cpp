#include "tiff.h"
#include <tiffio.h>
#include <cstdint>

static void s_errorHandler(const char*, const char*, va_list)
{
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

		::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, static_cast<std::uint32_t>(width));
		::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, static_cast<std::uint32_t>(height));
		::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, m_depth);
		::TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
		::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
		::TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, n_channels);
		::TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 1);
		::TIFFSetField(tiff, TIFFTAG_MINSAMPLEVALUE, static_cast<std::uint16_t>(0));
        ::TIFFSetField(tiff, TIFFTAG_MAXSAMPLEVALUE, static_cast<std::uint16_t>(255));
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
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint16_t chan_bits = 0;
		::TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
		::TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
		::TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &chan_bits);

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

