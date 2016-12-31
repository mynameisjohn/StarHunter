#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"
#include "Camera.h"

int main(int argc, char ** argv) {
    ImageSource::Ptr pImgSrc;
    ImageProcessor::Ptr pImgProc;
	
    //std::list<std::string> liInput;
    //for ( int i = 0; i < 5; i++ )
    //{
    //    liInput.push_back( "fakeStarImage_" + std::to_string( i + 1 ) + ".png" );
    //}

	//pImgSrc = ImageSource::Ptr( new SHCamera( "test", 10, 10 ) );// new FileReader_WithDrift( std::list<std::string>( { {"Untitled.png"} } ) ) );
 //   pImgProc = ImageProcessor::Ptr( new StarFinder() );

	//Engine E( std::move(pImgSrc), std::move(pImgProc) );
	//E.Run();

	return 0;
}
