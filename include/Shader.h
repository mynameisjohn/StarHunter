#pragma once

#include "GL_Util.h"

#include <string>
#include <map>

class Shader
{
public:
	Shader();
	bool Init( std::string strVertSrc, std::string strFragSrc, bool fromDisk );
	
	// Bound status
	bool Bind();
	bool Unbind();
	bool IsBound() const;

	// Logging functions
	int PrintLog_V() const;
	int PrintLog_F() const;
	int PrintSrc_V() const;
	int PrintSrc_F() const;
	int PrintLog_P() const;

	// Public Accessors
	GLint GetHandle( const std::string strVarName );

	// Scoped bind class
	class ScopedBind
	{
		friend class Shader;
	protected:
		Shader * m_pShader;
		ScopedBind( Shader * pS ) : m_pShader( pS ) { m_pShader->Bind(); }
	public:
		~ScopedBind() { m_pShader->Unbind(); }
	};
	ScopedBind ScopeBind() { return ScopedBind( this ); }

	static bool pylExpose();

private:
	// Bound status, program/shaders, source, handles
	bool m_bIsBound;
	GLuint m_Program;
	GLuint m_hVertShader;
	GLuint m_hFragShader;
	std::string m_VertShaderSrc, m_FragShaderSrc;
	std::map<std::string, GLint> m_mapHandles;
};