#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"

int main(int argc, char ** argv) {
	Engine E( ImageSource::Ptr( new FileReader( { "foo223.png", "foo224.png" } ) ), ImageProcessor::Ptr( new StarFinder_UI() ) );
	E.Run();

	return 0;
}
