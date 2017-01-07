#pragma once

#ifndef GL_INCLUDES
#define GL_INCLUDES

// Adds OpenGL, SDL2, and the glm forwards

#include <GL/glew.h>
#include <SDL_opengl.h>

// glm is important to me
#include <glm/fwd.hpp>
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::fquat;
using glm::mat4;
using glm::mat3;

// As are these printing functions
#include <iostream>
std::ostream& operator<<( std::ostream& os, const vec2& vec );
std::ostream& operator<<( std::ostream& os, const vec3& vec );
std::ostream& operator<<( std::ostream& os, const vec4& vec );
std::ostream& operator<<( std::ostream& os, const mat4& mat );
std::ostream& operator<<( std::ostream& os, const fquat& quat );

#endif //GL_INCLUDES
