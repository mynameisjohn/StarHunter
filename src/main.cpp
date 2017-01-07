#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"
#include "Camera.h"
#include "TelescopeComm.h"

#if SH_TELESCOPE
#include <pyliaison.h>
#endif

int main(int argc, char ** argv) 
{
	// Use the StarHunter code if we have both telescope and camera stuff
#if SH_CAMERA && SH_TELESCOPE
	StarHunter SH( 50, new SHCamera( "test", 10, 10 ), new TelescopeComm( "COM3" ), new StarFinder_Drift() );
	if ( SH.Run() )
		return 0;

	return -1;
#else
	std::list<std::string> liInput;
	for ( int i = 0; i < 5; i++ )
	{
		liInput.push_back( "fakeStarImage_" + std::to_string( i + 1 ) + ".png" );
	}

	std::unique_ptr<ImageSource> pImgSrc = ImageSource::Ptr( new FileReader_WithDrift( liInput ) );
	std::unique_ptr<ImageProcessor> pImgProc = ImageProcessor::Ptr( new StarFinder_UI() );
	
	Engine E( std::move( pImgSrc ), std::move( pImgProc ) );
	E.Run();

	return 0;	
#endif
}
