#pragma once

#include "Util.h"
#include "SDLGLWindow.h"
#include "Shader.h"
#include "GLCamera.h"
#include "Drawable.h"

class ImageTextureWindow : public SDLGLWindow
{
	Shader m_Shader;		// Shader object
	GLCamera m_GLCamera;	// Camera object
	Drawable m_PictureQuad;	// Picture quad

	// Cached dimensions for texture creation
	// The image will have to be single channel float
	int m_nImgWidth;
	int m_nImgHeight;

public:
	// Similar args to SDLGLWindow, but we load a shader program from disk and specify
	// what portion of the visible region the picture quad should occupy (0 < fQuadSize <= 1)
	ImageTextureWindow( std::string strName, int posX, int posY, int width, int height, int flags,
						int glMajor, int glMinor, bool bDoubleBuf,
						std::string strVertShader, std::string strFragShader,
						float fQuadSize );

	// Gets rid of texture resource
	~ImageTextureWindow();

	// Set the image being drawn
	bool SetImage( img_t img );

	// Draw an image if we have one
	void Draw() override;
};