#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"

int main(int argc, char ** argv) {
    ImageSource::Ptr pImgSrc;
    ImageProcessor::Ptr pImgProc;

    std::list<std::string> liInput;
    for ( int i = 0; i < 10; i++ )
    {
        liInput.push_back( "fakeStarImage_" + std::to_string( i + 1 ) + ".png" );
    }

    pImgSrc = ImageSource::Ptr( new FileReader_WithDrift( liInput ) );
    pImgProc = ImageProcessor::Ptr( new StarFinder_ImgOffset( (FileReader_WithDrift *) pImgSrc.get() ) );

	Engine E( std::move(pImgSrc), std::move(pImgProc) );
	E.Run();

	return 0;
}
