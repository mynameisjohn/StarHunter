#include "Engine.h"
#include "StarFinder.h"
#include "FileReader.h"
#include "Camera.h"

#if SH_TELESCOPE
#include <pyliaison.h>
#endif

int main(int argc, char ** argv) 
{
#if SH_CAMERA && SH_TELESCOPE
	try
	{
		// Args we expect on the CMD line are
		// SF DSP params?
		// Camera string prefix, exposure time, # of images
		// telescope port addr

		pyl::initialize();

		SHCamera shCamera( "test", 10, 10 );
		TelescopeComm shTelescope( "COM3" );
		StarFinder_Drift shStarFinder;

		// We'll use a state pattern here to try and get things done
		enum class State
		{
			NONE = 0,
			DETECT,	// No movement, scanning input for velocity
			CALIBRATE,	// Finding a good velocity value
			TRACK,		// Moving at good velocity, hopefully no change needed
			DONE
		};

		// Detect, then calibrate, then track, then get out
		for ( State eState = State::NONE; eState != State::DONE;)
		{
			// Variable declarations
			img_t img;
			float fDriftX( 0 ), fDriftY( 0 );
			int nSlewRateX( 0 ), nSlewRateY( 0 );
			ImageSource::Status eImgStat = ImageSource::Status::READY;

			switch ( eState )
			{
					// Initially set camera mode to streaming, set state to detect, and break
				case State::NONE:
					shCamera.SetMode( SHCamera::Mode::Streaming );
					eState = State::DETECT;
					break;
					// During the detect phase, we call GetNextImage on the camera
					// and send it to the star finder. Eventually the sf will get
					// a drift value, at which point we start calibrating
				case State::DETECT:
					// Get an image from the camera
					eImgStat = shCamera.GetNextImage( &img );
					if ( eImgStat == ImageSource::Status::READY )
						shStarFinder.HandleImage( img );
					else
						break;

					// This will return true once there are things with drift detected
					if ( shStarFinder.GetDrift_Cumulative( &fDriftX, &fDriftY ) )
					{
						// Switch to calibration state
						eState = State::CALIBRATE;
						break;
					}
					break;
					// The calibration state will change the slew rate of the
					// telescope mount until the drift values go below some threshold
				case State::CALIBRATE:
					// Get an image from the camera
					eImgStat = shCamera.GetNextImage( &img );
					if ( eImgStat == ImageSource::Status::READY )
						shStarFinder.HandleImage( img );
					else
						break;

					// Get the current drift value (this shouldn't return false...)
					if ( shStarFinder.GetDrift_Cumulative( &fDriftX, &fDriftY ) )
					{
						// If these are both below the threshold, set state to track
						if ( fabs( fDriftX ) < kEPS && fabs( fDriftY ) < kEPS )
						{
							eState = State::TRACK;
							break;
						}

						// Otherwise compute increments (1 in the direction of drift)
						shTelescope.GetSlewRate( &nSlewRateX, &nSlewRateY );
						if ( fabs( fDriftX ) < kEPS )
							nSlewRateX += fDriftX > 0 ? 1 : -1;
						if ( fabs( fDriftY ) < kEPS )
							nSlewRateY += fDriftY > 0 ? 1 : -1;

						// Set slew rate
						shTelescope.SetSlewRate( nSlewRateX, nSlewRateY );
					}
					break;
					// Not much for us to do in the tracking state, we are just
					// making the camera take images and storing them
				case State::TRACK:
					// This will return DONE when the camera is out of images
					if ( shCamera.GetNextImage( &img ) == ImageSource::Status::DONE )
					{
						eState = State::DONE;
						shCamera.SetMode( SHCamera::Mode::Off );
					}
					break;
			}
		}

		pyl::finalize();

	}
	catch ( const std::runtime_error& e )
	{
		std::cout << e.what() << std::endl;

		pyl::finalize();
		return -1;
	}
#endif

	return 0;
}
