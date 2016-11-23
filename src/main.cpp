#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"

int main(int argc, char ** argv) {
    ImageSource::Ptr pImgSrc;
    ImageProcessor::Ptr pImgProc;

    pImgSrc = ImageSource::Ptr(new FileReader_WithOfs( { "foo223.png", "foo224.png" } ));
    pImgProc = ImageProcessor::Ptr(new StarFinder_ImgOffset((FileReader_WithOfs *)pImgSrc.get()));

	Engine E( std::move(pImgSrc), std::move(pImgProc) );
	E.Run();

	return 0;
}
