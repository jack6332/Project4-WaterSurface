#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 proj_matrix;


void main()
{
    TexCoords = aTexCoords;
    gl_Position =proj_matrix * view_matrix * model_matrix * vec4(aPos, 1.0); 
}  