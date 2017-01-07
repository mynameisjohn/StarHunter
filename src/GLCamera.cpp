#include "GLCamera.h"

#include <algorithm>

// Declare static shader var
/*static*/ GLint GLCamera::s_CamMatHandle;

// This comes up enough
using glm::normalize;

void GLCamera::InitOrtho( int nScreenWidth, int nScreenHeight, float xMin, float xMax, float yMin, float yMax )
{
	Reset();
	m_eType = Type::ORTHO;
	m_nScreenWidth = std::max( 0, nScreenWidth );
	m_nScreenHeight = std::max( 0, nScreenHeight );
	m_m4Proj = glm::ortho( xMin, xMax, yMin, yMax );
}

void GLCamera::InitPersp( int nScreenWidth, int nScreenHeight, float fovy, float aspect, float near, float far )
{
	Reset();
	m_eType = Type::PERSP;
	m_nScreenWidth = std::max( 0, nScreenWidth );
	m_nScreenWidth = std::max( 0, nScreenHeight );
	m_m4Proj = glm::perspective( fovy, aspect, near, far );
}

int GLCamera::GetScreenWidth() const
{
	return m_nScreenWidth;
}

int GLCamera::GetScreenHeight() const
{
	return m_nScreenHeight;
}

float GLCamera::GetAspectRatio() const
{
	if ( m_nScreenHeight > 0 )
		return float( m_nScreenWidth ) / float( m_nScreenHeight );
	return 0.f;
}

// See how this would affect a vector pointing out in z
vec3 GLCamera::GetView() const
{
	return vec3( m_m4Proj * vec4( 0, 0, 1, 1 ) );
}

GLCamera::GLCamera()
{
	Reset();
}

void GLCamera::Reset()
{
	m_eType = Type::NONE;
	m_nScreenWidth = 0;
	m_nScreenHeight = 0;
	ResetTransform();
	ResetProj();
}

void GLCamera::ResetPos()
{
	m_qvTransform.vec = vec3( 0 );
}

void GLCamera::ResetRot()
{
	m_qvTransform.quat = fquat( 1, 0, 0, 0 );
}

void GLCamera::ResetTransform()
{
	m_qvTransform = quatvec( quatvec::Type::RT );
}

void GLCamera::ResetProj()
{
	m_m4Proj = mat4( 1 );
}

// Get at the quatvec
vec3 GLCamera::GetPos() const
{
	return m_qvTransform.vec;
}
fquat GLCamera::GetRot() const
{
	return m_qvTransform.quat;
}
quatvec GLCamera::GetTransform() const
{
	return m_qvTransform;
}
mat4 GLCamera::GetTransformMat() const
{
	return m_qvTransform.ToMat4();
}

// return proj as is
mat4 GLCamera::GetProjMat() const
{
	return m_m4Proj;
}

mat4 GLCamera::GetCameraMat() const
{
	return GetProjMat() * GetTransformMat();
}

// These may be wrong, but I have to figure out why
void GLCamera::Translate( vec3 t )
{
	m_qvTransform.vec += t;
}

void GLCamera::Translate( vec2 t )
{
	m_qvTransform.vec += vec3( t, 0 );
}

void GLCamera::Rotate( fquat q )
{
	m_qvTransform.quat *= q;
}

/*static*/ void GLCamera::SetCamMatHandle( GLint h )
{
	s_CamMatHandle = h;
}

/*static*/ GLint GLCamera::GetCamMatHandle()
{
	return s_CamMatHandle;
}
