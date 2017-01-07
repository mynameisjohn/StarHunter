#pragma once
#include "GL_Util.h"
#include "quatvec.h"

class GLCamera
{
public:
	enum class Type
	{
		ORTHO,
		PERSP,
		NONE
	};
	GLCamera();
	void InitOrtho( int nScreenWidth, int nScreenHeight, float xMin, float xMax, float yMin, float yMax );
	void InitPersp( int nScreenWidth, int nScreenHeight, float fovy, float aspect, float near, float far );

	void ResetRot();
	void ResetPos();
	void ResetTransform();
	void ResetProj();
	void Reset();

	int GetScreenWidth() const;
	int GetScreenHeight() const;
	float GetAspectRatio() const;
	vec3 GetView() const;
	vec3 GetPos() const;
	fquat GetRot() const;
	quatvec GetTransform() const;
	mat4 GetProjMat() const;
	mat4 GetTransformMat() const;
	mat4 GetCameraMat() const;

	void Translate( vec3 t );
	void Translate( vec2 t );
	void Rotate( fquat q );

	static void SetCamMatHandle( GLint h );
	static GLint GetCamMatHandle();

	static bool pylExpose();

private:
	Type m_eType;
	int m_nScreenWidth;
	int m_nScreenHeight;
	quatvec m_qvTransform;
	mat4 m_m4Proj;

	static GLint s_CamMatHandle;
};
