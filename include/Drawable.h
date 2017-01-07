#pragma once

#include "GL_Util.h"
#include "quatvec.h"

#include <map>
#include <array>

class Drawable
{
public:
	Drawable();
	bool Init( std::string strName, std::array<glm::vec3, 4> quadVerts, glm::vec4 v4Color, quatvec qvTransform, glm::vec2 v2Scale );

	glm::vec4 GetColor() const;
	glm::vec3 GetPos() const;
	glm::fquat GetRot() const;
	quatvec GetTransform() const;
	glm::mat4 GetMV() const;

	void SetPos3D( glm::vec3 t );
	void Translate3D( glm::vec3 t );

	void SetPos2D( glm::vec2 t );
	void Translate2D( glm::vec2 t );

	void SetRot( glm::fquat q );
	void Rotate( glm::fquat q );

	void SetTransform( quatvec qv );
	void Transform( quatvec qv );

	void SetScale( vec2 S );
	void Scale( vec2 S );
	void Scale ( float s );

	void SetTexID( GLuint nTexID );
	GLuint GetTexID() const;

	void SetColor( glm::vec4 c );

	bool Draw();

	static void SetPosHandle( GLint h );
	static GLint GetPosHandle();

	static void SetTexHandle( GLint h );
	static GLint GetTexHandle();

	static void SetColorHandle( GLint h );
	static GLint GetColorHandle();

	void SetIsActive( bool b );
	bool GetIsActive() const;

	using VAOData = std::array<GLuint, 2>;
private:	
	bool m_bActive;
	GLuint m_VAO;
	GLuint m_nIdx;
	GLuint m_nTexID;

	glm::vec2 m_v2Scale;
	glm::vec4 m_v4Color;
	quatvec m_qvTransform;
	std::string m_strSrcFile;

	// Static VAO cache (string to VAO/nIdx)
	static std::map<std::string, VAOData> s_VAOCache;

	// Static Drawable Cache for common primitives
	static std::map<std::string, Drawable> s_PrimitiveMap;

	// Shader handles for position and color
	static GLint s_PosHandle;
	static GLint s_ColorHandle;
	static GLint s_TexHandle;
};