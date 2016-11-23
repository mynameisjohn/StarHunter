#pragma once

#include "Engine.h"
#include <list>
#include <initializer_list>

// FileReader class, reads in a list of image files and streams them
class FileReader : public ImageSource
{
	std::list<std::string> m_liFileNames;
public:
	FileReader( std::initializer_list<std::string> liFileNames );
	bool HasImages() const override;
	cv::Mat GetNextImage() override;
};

// Like above, but a pixel offset can be applied
// to images (meant to simulate a moving camera)
class FileReader_WithOfs : public FileReader{
    int m_nOfsX;
    int m_nOfsY;
public:
    FileReader_WithOfs(std::initializer_list<std::string> liFileNames);
    cv::Mat GetNextImage() override;
    void SetOffset(int nOfsX, int nOfsY);
    void GetOffset(int * pnOfsX, int * pnOfsY);
};
