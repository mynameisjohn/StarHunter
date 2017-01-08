
#include "ImageTextureWindow.h"
#include <glm/gtc/type_ptr.hpp>

ImageTextureWindow::ImageTextureWindow( std::string strName, int posX, int posY, int width, int height, int flags,
										int glMajor, int glMinor, bool bDoubleBuf,
										std::string strVertShader, std::string strFragShader,
										float fQuadSize ) :
	SDLGLWindow( strName, posX, posY, width, height, flags, glMajor, glMinor, bDoubleBuf ),
	m_nImgHeight( 0 ),
	m_nImgWidth( 0 )
{
	// Init shader, camera
	if ( !m_Shader.Init( strVertShader, strFragShader, true ) )
		throw std::runtime_error( "error constructing shader object!" );

	m_GLCamera.InitOrtho( width, height, -1, 1, -1, 1 );

	// Bind shader
	auto sBind = m_Shader.ScopeBind();

	// Set shader variable handles
	Drawable::SetPosHandle( m_Shader.GetHandle( "a_Pos" ) );
	Drawable::SetTexHandle( m_Shader.GetHandle( "a_Tex" ) );
	Drawable::SetColorHandle( m_Shader.GetHandle( "u_Color" ) );

	// Create drawable with texture coords, clamped to window size
	fQuadSize = std::min( std::max( fQuadSize, 0.f ), 1.f );
	std::array<glm::vec3, 4> arTexCoords {
		glm::vec3( -fQuadSize, -fQuadSize, 0 ),
		glm::vec3( fQuadSize, -fQuadSize, 0 ),
		glm::vec3( fQuadSize, fQuadSize, 0 ),
		glm::vec3( -fQuadSize, fQuadSize, 0 )
	};
	m_PictureQuad.Init( "PictureQuad", arTexCoords, vec4( 1 ), quatvec(), vec2( 1, 1 ) );
}

ImageTextureWindow::~ImageTextureWindow()
{
	GLuint uTexID = m_PictureQuad.GetTexID();
	if ( uTexID )
	{
		// ? I should know how to do this
		glDeleteTextures( 1, &uTexID );
		m_PictureQuad.SetTexID( 0 );
	}
}

bool ImageTextureWindow::SetImage( img_t img )
{
	// Image must be single channel float
	if ( img.channels() != 1 || img.type() != CV_32FC1 )
	{
		throw std::runtime_error( "Error: Invalid image type for opengl texture!" );
		return false;
	}

	// Create texture if needed
	GLuint uTexID = m_PictureQuad.GetTexID();
	if ( uTexID == 0 )
	{
		//Generate the device texture and bind it
		glGenTextures( 1, &uTexID );
		if ( uTexID == 0 )
		{
			throw std::runtime_error( "Error creating opengl texture!" );
			return false;
		}

		// Set texture ID
		m_PictureQuad.SetTexID( uTexID );

		// Cache image dimension stuff
		m_nImgWidth = img.cols;
		m_nImgHeight = img.rows;

		// Bind texture
		glBindTexture( GL_TEXTURE_2D, uTexID );

		// Upload host texture to device (single channel float)
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RED, m_nImgWidth, m_nImgHeight, 0, GL_RED, GL_FLOAT, img.ptr() );

		// Set filtering   
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	// We have an opengl texture
	else
	{
		// Otherwise make sure this matches with our dimensions
		if ( img.cols != m_nImgWidth || img.rows != m_nImgHeight )
		{
			throw std::runtime_error( "Error: invalid incoming image!" );
			return false;
		}

		// All good? Bind the texture
		glBindTexture( GL_TEXTURE_2D, uTexID );
	}

	// Upload image (single channel float)
	glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_nImgWidth, m_nImgHeight, GL_RED, GL_FLOAT, img.ptr() );
	glBindTexture( GL_TEXTURE_2D, 0 );

	// Unbind (?)
	glBindTexture( GL_TEXTURE_2D, 0 );
	return true;
}

void ImageTextureWindow::Draw()
{
	// Only draw if we have a texture ID
	if ( m_PictureQuad.GetTexID() )
	{
		// Declare scoped objects
		Updater update( GetWindow() );
		Shader::ScopedBind sBind = m_Shader.ScopeBind();

		// Get shader handles
		GLuint pmvHandle = m_Shader.GetHandle( "u_PMV" );
		GLuint clrHandle = m_Shader.GetHandle( "u_Color" );

		// Get transform matrices, color, for upload
		mat4 P = m_GLCamera.GetCameraMat();
		mat4 PMV = P * m_PictureQuad.GetMV();
		vec4 c = m_PictureQuad.GetColor();

		// Upload data
		glUniformMatrix4fv( pmvHandle, 1, GL_FALSE, glm::value_ptr( PMV ) );
		glUniform4fv( clrHandle, 1, glm::value_ptr( c ) );

		// Draw (this binds the texture ID), then unbind and get out
		m_PictureQuad.Draw();
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
}