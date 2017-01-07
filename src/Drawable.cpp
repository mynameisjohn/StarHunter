#include "Drawable.h"

#include <string>
#include <vector>

// Static var declarations
/*static*/ GLint Drawable::s_PosHandle;
/*static*/ GLint Drawable::s_ColorHandle;
/*static*/ GLint Drawable::s_TexHandle;
/*static*/ std::map<std::string, Drawable::VAOData> Drawable::s_VAOCache;
/*static*/ std::map<std::string, Drawable> Drawable::s_PrimitiveMap;

Drawable::Drawable() :
	m_bActive( false ),
	m_VAO( 0 ),
	m_nIdx( 0 ),
	m_nTexID( 0 ),
	m_v2Scale( 1 ),
	m_v4Color( 1 ),
	m_qvTransform( quatvec::Type::TRT )
{}

// Function for getting data into a Vertex Buffer Object
void fillVBO( GLuint buf, GLint handle, void * ptr, GLsizeiptr numBytes, GLuint dim, GLuint type )
{
	glBindBuffer( GL_ARRAY_BUFFER, buf );
	glBufferData( GL_ARRAY_BUFFER, numBytes, ptr, GL_STATIC_DRAW );
	glEnableVertexAttribArray( handle );
	glVertexAttribPointer( handle, dim, type, 0, 0, 0 );
	//Disable?
}

bool Drawable::Init( std::string strName, std::array<glm::vec3, 4> quadVerts, glm::vec4 v4Color, quatvec qvTransform, glm::vec2 v2Scale)
{
	if ( Drawable::s_PosHandle < 0 )
	{
		std::cerr << "Error: you haven't initialized the static pos handle for drawables!" << std::endl;
		return false;
	}

	// See if we've loaded this Iqm File before
	if ( s_VAOCache.find( strName ) == s_VAOCache.end() )
	{
		// Try and construct the drawable from an IQM file
		try
		{
			// We'll be creating an indexed array of VBOs
			GLuint VAO( 0 ), nIdx( 0 );

			// Create vertex array object
			glGenVertexArrays( 1, &VAO );
			if ( VAO == 0 )
			{
				std::cerr << "Error creating VAO for " << strName << std::endl;
				return false;
			}

			// Bind if successful
			glBindVertexArray( VAO );

			// Create VBOs
			std::array<GLuint, 3> vboBuf{ { 0, 0, 0 } };
			glGenBuffers( vboBuf.size(), vboBuf.data() );
			if ( vboBuf[0] == 0 || vboBuf[1] == 0 || vboBuf[2] == 0 )
			{
				std::cerr << "Error creating VBOs " << strName << std::endl;
				return false;
			}

			// If successful, bind position attr and upload data
			GLuint bufIdx( 0 );
			fillVBO( vboBuf[bufIdx++], s_PosHandle, quadVerts.data(), quadVerts.size() * sizeof( glm::vec3 ), 3, GL_FLOAT );

			std::array<vec2, 4> arTexCoords{ 
				vec2( 0, 1 ), 
				vec2( 1, 1 ), 
				vec2( 1, 0 ), 
				vec2( 0, 0 ) };
			fillVBO( vboBuf[bufIdx++], s_TexHandle, arTexCoords.data(), arTexCoords.size() * sizeof( glm::vec2 ), 2, GL_FLOAT );

			// Same for indices
			std::vector<GLuint> indices = { 0, 1, 3, 1, 3, 2 };
			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vboBuf[bufIdx] );
			glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof( GLuint ), indices.data(), GL_STATIC_DRAW );
			
			// Unbind VAO and cache data
			glBindVertexArray( 0 );

			s_VAOCache[strName] = { VAO, (GLuint) indices.size() };
		}
		catch ( std::runtime_error )
		{
			std::cerr << "Error constructing drawable from " << strName << std::endl;
			return false;
		}
	}

	// Store the transform and color values
	m_qvTransform = qvTransform;
	m_v2Scale = v2Scale;
	m_v4Color = v4Color;
	m_bActive = true;

	// Store the values from the static cache, return true
	m_VAO = s_VAOCache[strName][0];
	m_nIdx = s_VAOCache[strName][1];

	return true;
}

void Drawable::SetIsActive( bool b )
{
	m_bActive = b;
}

bool Drawable::GetIsActive() const
{
	return m_bActive;
}

vec4 Drawable::GetColor() const
{
	return m_v4Color;
}

vec3 Drawable::GetPos() const
{
	return m_qvTransform.vec;
}

fquat Drawable::GetRot() const
{
	return m_qvTransform.quat;
}

quatvec Drawable::GetTransform() const
{
	return m_qvTransform;
}

mat4 Drawable::GetMV() const
{
	return m_qvTransform.ToMat4() * glm::scale( vec3( m_v2Scale, 1.f ) );
}

void Drawable::SetPos3D( vec3 t )
{
	m_qvTransform.vec = t;
}

void Drawable::Translate3D( vec3 t )
{
	m_qvTransform.vec += t;
}

void Drawable::SetPos2D( vec2 t )
{
	m_qvTransform.vec = vec3( t, 0 );
}

void Drawable::Translate2D( vec2 t )
{
	m_qvTransform.vec += vec3( t, 0 );
}

void Drawable::SetRot( fquat q )
{
	m_qvTransform.quat = q;
}

void Drawable::Rotate( fquat q )
{
	m_qvTransform.quat *= q;
}

void Drawable::SetTransform( quatvec qv )
{
	m_qvTransform = qv;
}

void Drawable::Transform( quatvec qv )
{
	m_qvTransform *= qv;
}

void Drawable::Scale( vec2 s )
{
	m_v2Scale *= s;
}

void Drawable::Scale( float s )
{
	m_v2Scale *= s;
}

void Drawable::SetScale( vec2 s )
{
	m_v2Scale = s;
}

void Drawable::SetColor( vec4 c )
{
	m_v4Color = glm::clamp( c, vec4( 0 ), vec4( 1 ) );
}

bool Drawable::Draw()
{
	if ( s_PosHandle < 0 || s_ColorHandle < 0 )
	{
		std::cerr << "Error! Static drawable handles not set!" << std::endl;
		return false;
	}

	// Bind VAO, draw, don't bother unbinding
	// We could upload the color and MV here,
	// but because P*MV can be done beforehand
	// it's kind of an optimization to leave it outside
	glBindVertexArray( m_VAO );
	if ( m_nTexID )
		glBindTexture( GL_TEXTURE_2D, m_nTexID );
	glDrawElements( GL_TRIANGLES, m_nIdx, GL_UNSIGNED_INT, NULL );
	glBindVertexArray( m_VAO );

	return true;
}

/*static*/ void Drawable::SetPosHandle( GLint pH )
{
	s_PosHandle = pH;
}

/*static*/ GLint Drawable::GetPosHandle()
{
	return s_PosHandle;
}

/*static*/ void Drawable::SetTexHandle( GLint tH )
{
	s_TexHandle = tH;
}

/*static*/ GLint Drawable::GetTexHandle()
{
	return s_TexHandle;
}

/*static*/ void Drawable::SetColorHandle( GLint cH )
{
	s_ColorHandle = cH;
}

/*static*/ GLint Drawable::GetColorHandle()
{
	return s_ColorHandle;
}

void Drawable::SetTexID( GLuint nTexID )
{
	m_nTexID = nTexID;
}

GLuint  Drawable::GetTexID() const
{
	return m_nTexID;
}