#include "Shader.h"

#include <fstream>
#include <iostream>

Shader::Shader() :
	m_bIsBound( false ),
	m_Program( 0 ),
	m_hVertShader( 0 ),
	m_hFragShader( 0 )
{}

bool Shader::Init( std::string strVertSrc, std::string strFragSrc, bool fromDisk )
{
	if ( fromDisk )
	{
		using ibufiter = std::istreambuf_iterator<char>;
		std::ifstream vIn( strVertSrc ), fIn( strFragSrc );
		if ( vIn.good() && fIn.good() )
		{
			strVertSrc = std::string ( ibufiter( vIn ), ibufiter() );
			strFragSrc = std::string ( ibufiter( fIn ), ibufiter() );
		}
		else
			return false;
	}

	if ( strVertSrc.empty() || strFragSrc.empty() )
		return false;

	m_VertShaderSrc = strVertSrc;
	m_FragShaderSrc = strFragSrc;

	// Check if the shader op went ok
	auto check = [] ( GLuint id, GLuint type )
	{
		GLint status( GL_FALSE );
		if ( type == GL_COMPILE_STATUS )
			glGetShaderiv( id, type, &status );
		if ( type == GL_LINK_STATUS )
			glGetProgramiv( id, type, &status );
		return status == GL_TRUE;
	};

	// Weird thing that helps with glShaderSource
	const GLchar * shaderSrc[] = { m_VertShaderSrc.c_str(), m_FragShaderSrc.c_str() };

	// Compile Vertex Shader
	m_hVertShader = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( m_hVertShader, 1, &(shaderSrc[0]), 0 );
	glCompileShader( m_hVertShader );
	if ( !check( m_hVertShader, GL_COMPILE_STATUS ) )
	{
		std::cout << "Unable to compile vertex shader." << std::endl;
		PrintLog_V();
		return false;
	}

	// Compile Frag Shader
	m_hFragShader = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( m_hFragShader, 1, &(shaderSrc[1]), 0 );
	glCompileShader( m_hFragShader );
	if ( !check( m_hFragShader, GL_COMPILE_STATUS ) )
	{
		std::cout << "Unable to compile fragment shader." << std::endl;
		PrintLog_F();
		return false;
	}

	// Create and Link Program
	m_Program = glCreateProgram();
	glAttachShader( m_Program, m_hVertShader );
	glAttachShader( m_Program, m_hFragShader );
	glLinkProgram( m_Program );
	if ( !check( m_Program, GL_LINK_STATUS ) )
	{
		PrintLog_P();
		std::cout << "Unable to link shader program." << std::endl;
		return false;
	}

	// Get all uniform and attribute handles now
	ScopedBind sBind = ScopeBind();

	GLint nUniforms( 0 ), nAttributes( 0 );
	const GLsizei uMaxNumChars = 256;
	GLchar szNameBuf[uMaxNumChars]{ 0 };
	GLsizei uLen( 0 );
	GLint iSize( 0 );
	GLenum eType( 0 );

	glGetProgramiv( m_Program, GL_ACTIVE_UNIFORMS, &nUniforms );
	glGetProgramiv( m_Program, GL_ACTIVE_ATTRIBUTES, &nAttributes );

	for ( int i = 0; i < nUniforms; i++ )
	{
		memset( szNameBuf, 0, sizeof( szNameBuf ) );
		glGetActiveUniform( m_Program, i, uMaxNumChars, &uLen, &iSize, &eType, szNameBuf );
		m_mapHandles[szNameBuf] = i;
	}

	for ( int i = 0; i < nAttributes; i++ )
	{
		memset( szNameBuf, 0, sizeof( szNameBuf ) );
		glGetActiveAttrib( m_Program, i, uMaxNumChars, &uLen, &iSize, &eType, szNameBuf );
		m_mapHandles[szNameBuf] = i;
	}

	return true;
}

// Managing bound state
bool Shader::Bind()
{
	if ( !m_bIsBound )
	{
		glUseProgram( m_Program );
		m_bIsBound = true;
	}
	return m_bIsBound == true;
}

bool Shader::Unbind()
{
	if ( m_bIsBound )
	{
		glUseProgram( 0 );
		m_bIsBound = false;
	}
	return m_bIsBound == false;
}

bool Shader::IsBound() const
{
	return m_bIsBound;
}

// Accessor for shader handles
GLint Shader::GetHandle( const std::string strVarName )
{
	// If we have the handle, return it
	if ( m_mapHandles.find( strVarName ) != m_mapHandles.end() )
		return m_mapHandles.find( strVarName )->second;

	// Otherwise return -1
	std::cerr << "Error! Invalid shader variable " << strVarName << " queried!" << std::endl;
	return -1;
}

// Print Logs
int Shader::PrintLog_V() const
{
	const int max( 1024 );
	int len( 0 );
	char log[max];
	glGetShaderInfoLog( m_hVertShader, max, &len, log );
	std::cout << "Vertex Shader Log: \n\n" << log << "\n\n" << std::endl;

	return len;
}

int Shader::PrintLog_F() const
{
	const int max( 1024 );
	int len( 0 );
	char log[max];
	glGetShaderInfoLog( m_hFragShader, max, &len, log );
	std::cout << "Fragment Shader Log: \n\n" << log << "\n\n" << std::endl;

	return len;
}

int Shader::PrintLog_P() const
{
	const int max( 1024 );
	int len( 0 );
	char log[max]{ 0 };
	glGetShaderInfoLog( m_Program, max, &len, log );
	std::cout << "Fragment Shader Log: \n\n" << log << "\n\n" << std::endl;

	return len;
}

// Print Source
int Shader::PrintSrc_V() const
{
	std::cout << "Vertex Shader Source: \n\n" << m_VertShaderSrc << "\n\n" << std::endl;
	return m_VertShaderSrc.length();
}

int Shader::PrintSrc_F() const
{
	std::cout << "Fragment Shader Source: \n\n" << m_FragShaderSrc << "\n\n" << std::endl;
	return m_FragShaderSrc.length();
}