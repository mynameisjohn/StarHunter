#include "Engine.h"

#include <memory>
#include <vector>
#include <utility>

#include <opencv2/opencv.hpp>

// Circle struct - made my
// own so it can be constructed
// in host and device code
struct Circle
{
	float fX;	// x pos
	float fY;	// y pos
	float fR;	// radius
};

// Base star finder class
// All it's for is calling findStars,
// which performs the signal processing
// and leaves m_dBoolImg with a byte
// image where nonzero pixels are stars
class StarFinder : public ImageProcessor
{
protected:
	// Processing params
	float m_fFilterRadius;
	float m_fDilationRadius;
	float m_fHWHM;
	float m_fIntensityThreshold;

	// The images we use and their size
	img_t m_imgInput;
	img_t m_imgGaussian;
	img_t m_imgTopHat;
	img_t m_imgPeak;
	img_t m_imgThreshold;
	img_t m_imgDilated;
	img_t m_imgLocalMax;
	img_t m_imgStars;
	img_t m_imgBoolean;
	img_t m_imgTmp;

	// Leaves bool image with star locations
	bool findStars( img_t& img );

public:
	// TODO work out some algorithm parameters,
	// it's all hardcoded nonsense right now
	StarFinder();

	bool HandleImage( img_t img ) override;

};

// UI implementation - pops opencv window
// with trackbars to control parameters and
// see results
class StarFinder_UI : public StarFinder
{
public:
	StarFinder_UI();
	bool HandleImage( img_t img ) override;
};

// Implementation that computes the average
// drift in star positions across each
// handled image. 
class StarFinder_Drift : public StarFinder
{
	// The drift will be averaged when requested
	int m_nImagesProcessed;
    float m_fDriftX_Prev;
    float m_fDriftY_Prev;
	float m_fDriftX_Cumulative;
	float m_fDriftY_Cumulative;

	// The last set of stars we found
	// We'll be looking for their match
	std::vector<Circle> m_vLastCircles;
public:
	StarFinder_Drift();
	bool HandleImage( img_t img ) override;
    bool GetDrift_Prev( float * pDriftX, float * pDriftY ) const;
    bool GetDrift_Cumulative( float * pDriftX, float * pDriftY ) const;
};

// Same as above, but drift values are sent to
// a FileReader_WithOfs instance to simulate
// telescope mount movement
class FileReader_WithDrift;
class StarFinder_ImgOffset : public StarFinder_Drift
{
    FileReader_WithDrift * m_pFileReader;

public:
    StarFinder_ImgOffset( FileReader_WithDrift * pFileReader);
    bool HandleImage( img_t img ) override;
};

#if SH_TELESCOPE && SH_CAMERA

class SHCamera;
class TelescopeComm;
class StarHunter
{
public:
	// We'll use a state pattern here to try and get things done
	enum class State
	{
		NONE = 0,
		DETECT,		// No movement, scanning input for velocity
		CALIBRATE,	// Finding a good velocity value
		TRACK,		// Moving at good velocity, hopefully no change needed
		DONE		// We're all done
	};

	StarHunter( SHCamera * pCamera, TelescopeComm * pTelescopeComm, StarFinder_Drift * pStarFinder );
	bool Run();

private:
	State m_eState;
	std::unique_ptr<SHCamera> m_upCamera;
	std::unique_ptr<TelescopeComm> m_upTelescopeComm;
	std::unique_ptr<StarFinder_Drift> m_upStarFinder;
};

#endif // SH_TELESCOPE && SH_CAMERA

// Finds overlapping circles and combines them
std::vector<Circle> CollapseCircles( const std::vector<Circle>& vInput );

// Takes in boolean star image and returns a vector of stars as circles
std::vector<Circle> FindStarsInImage( float fStarRadius, img_t& dBoolImg );
