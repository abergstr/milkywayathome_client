#version 150

uniform mat4 cameraToClipMatrix;
uniform mat4 modelToCameraMatrix;

in vec4 cmPos;

void main()
{
    gl_Position = cameraToClipMatrix * modelToCameraMatrix * cmPos;
}

