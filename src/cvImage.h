#ifndef CVX_IMAGE_H
#define CVX_IMAGE_H

#include <opencv2/opencv.hpp>
#include <opencv2/core/types_c.h>
#include <opencv2/core/core_c.h>
#undef min
#undef max

inline int iplChannelBytes(const IplImage *image)
{
	return (image->depth & 0xFF) / 8;
}

template <class T>
T *iplImageRow(IplImage *image, int y)
{
	return (T *)(image->imageData + y * image->widthStep);
}

template <class T>
const T *iplImageRow(const IplImage *image, int y)
{
	return (const T *)(image->imageData + y * image->widthStep);
}

template <class T>
T *iplImageRow(IplImage *image, int y, int x)
{
	return (T *)(image->imageData + y * image->widthStep + (x * image->nChannels * (image->depth & 0xFF) / 8));
}

template <class T>
const T *iplImageRow(const IplImage *image, int y, int x)
{
	return (const T *)(image->imageData + y * image->widthStep + (x * image->nChannels * (image->depth & 0xFF) / 8));
}

class cvImageRef
{
public:
	cvImageRef() : m_image(NULL), m_header(false) {}
	cvImageRef(IplImage *image, bool header = false) : m_image(image), m_header(header) {}

	cvImageRef &operator=(IplImage *image)
	{
		if (m_image)
        {
            if (m_header)
                cvReleaseImageHeader(&m_image);
            else
                cvReleaseImage(&m_image);
        }
		m_image = image;
		return *this;
	}

	~cvImageRef()
	{
		if (m_image)
		{
			if (m_header)
				cvReleaseImageHeader(&m_image);
			else
				cvReleaseImage(&m_image);
		}
	}

	cvImageRef &operator=(cvImageRef &rhs)
	{
		if (this != &rhs)
		{
			if (m_image)
			{
				if (m_header)
					cvReleaseImageHeader(&m_image);
				else
					cvReleaseImage(&m_image);
			}
			m_image = rhs.m_image;
            m_header = rhs.m_header;
			rhs.m_image = NULL;
		}
		return *this;
	}

	IplImage *get() const { return m_image; }

	operator IplImage *()
	{
		assert(m_image);
		return m_image;
	}
	operator const IplImage *() const
	{
		assert(m_image);
		return m_image;
	}
	IplImage * operator->() { assert(m_image); return m_image; }
	const IplImage * operator->() const { assert(m_image); return m_image; }

	IplImage *detach()
	{
		IplImage *result = m_image;
		m_image = NULL;
		return result;
	}

	uchar *begin() { return (uchar *)m_image->imageData; }
	uchar *end() { return (uchar *)m_image->imageData + m_image->imageSize; }
	const uchar *begin() const { return (uchar *)m_image->imageData; }
	const uchar *end() const { return (uchar *)m_image->imageData + m_image->imageSize; }

private:
	IplImage *m_image;
  bool m_header;
};

#endif


