#include "FileReader.h"

// FileReader class, too simple to get its own file
class FileReader : public ImageSource
{
	std::list<std::string> m_liFileNames;
public:
	FileReader( std::initializer_list<std::string> liFileNames );
	bool HasImages() const override;
	cv::Mat GetNextImage() override;
};

FileReader::FileReader( std::initializer_list<std::string> liFileNames ) :
	m_liFileNames( liFileNames )
{}

bool FileReader::HasImages() const
{
	return m_liFileNames.empty() == false;
}

cv::Mat FileReader::GetNextImage()
{
	if ( !HasImages() )
		throw std::runtime_error( "Error: FileReader has no more images!" );
	
	cv::Mat matRet = cv::imread( m_liFileNames.front() );
	m_liFileNames.pop_front();

	return matRet;
}

class FileReader_WithOfs : public FileReader{
    int m_nOfsX;
    int m_nOfsY;
public:
    FileReader_WithOfs(std::initializer_list<std::string> liFileNames);
    cv::Mat GetNextImage() override;
};

FileReader_WithOfs::FileReader_WithOfs(std::initializer_list<std::string> liFileNames):
    FileReader(liFileNames),
    m_nOfsX(0),
    m_nOfsY(0)
{}

void FileReader_WithOfs::SetOffset(int nOfsX, int nOfsY){
    m_nOfsX = std::max(0, nOfsX);
    m_nOfsY = std::max(0, nOfsY);
}

void FileReader_WithOfs::GetOffset(int * pnOfsX, int * pnOfsY) const{
    if (pnOfsX)
        *pnOfsX = m_nOfsX;
    if (pnOfsY)
        *pnOfsY = m_nOfsY;
}

cv::Mat FileReader_WithOfs::GetNextImage(){
    // Get next image
    cv::Mat img = FileReader::GetNextImage();

    // Apply the inverse offset to the image, i.e
    // if our offset is 10 pixels up, move the image
    // 10 pixels down (to simulate our viewpoint moving)
    size_t uOfsX = std::min(m_uOfsX, (size_t)img.cols);
    size_t uOfsY = std::min(m_uOfsY, (size_t)img.rows);

    // Create black image
    cv::Mat ret(cv::Mat::zeros(img.size(), img.type()));
    // Get sub image of original
    cv::Mat subImg = img(cv::Rect(uOfsX, uOfsY, img.cols-uOfsX, img.rows-uOfsY));
    // Copy partial image into black image
    subImg.copyTo(ret(cv::Rect(0, 0, img.cols-uOfsX, img.rows-uOfsY)));
    
    return ret;
}
