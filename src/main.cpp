#include "Engine.h"
#include "StarFinder.h"

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
#include <stdio.h>
int main(int argc, char ** argv) {
	Engine E( ImageSource::Ptr( new FileReader( { "foo223.png" } ) ), ImageProcessor::Ptr( new StarFinder_UI() ) );
	E.Run();

	return 0;
}