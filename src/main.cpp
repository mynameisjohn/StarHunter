#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"
#include "Camera.h"

int main(int argc, char ** argv) {
    ImageSource::Ptr pImgSrc;
    ImageProcessor::Ptr pImgProc;

    std::list<std::string> liInput;
    for ( int i = 0; i < 5; i++ )
    {
        liInput.push_back( "foo" + std::to_string( i ) + ".cr2" );
    }

    pImgSrc = ImageSource::Ptr( new FileReader_WithDrift( liInput ) );
//    pImgSrc = ImageSource::Ptr( new SHCamera() );
    pImgProc = ImageProcessor::Ptr( new StarFinder_ImgOffset( (FileReader_WithDrift *) pImgSrc.get() ) );

	Engine E( std::move(pImgSrc), std::move(pImgProc) );
	E.Run();

	return 0;
}
