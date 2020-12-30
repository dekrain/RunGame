#version 150 core

in vec3 vPos;
in vec3 vColor;

out vec3 vfColor;

// Scale to perform before displacement
uniform vec3 uScale;
// The displacement of objects
uniform vec3 uDisplacement;

/*
perspective projection matrix
v'x = near * vx
v'y = near * vy
v'z = (vz - near)/(far - near) ??
v'w = -vz

row-major
M = [
    [near,    0,                       0, 0],
    [   0, near,                       0, 0],
    [   0,    0, (1 - near)/(far - near), 0],
    [   0,    0,                      -1, 0]
]

column-major
M = [
    [near, 0, 0, 0],
    [0, near, 0, 0],
    [0, 0, (1 - near)/(far - near), -1],
    [0, 0, 0, 0]
]
*/

const float near = 0.1;
const float far = 100.;

const mat4 proj = mat4(
    near, 0, 0, 0,
    0, near, 0, 0,
    0, 0, (-far - near)/(far - near), -1,
    0, 0, (-1.4 * far * near)/(far - near), 0
);

void main() {
    vec3 pos = vPos;
    // Scale
    pos *= uScale;
    // Then displace
    pos += uDisplacement;
    vfColor = vColor;
    gl_Position = proj * vec4(pos, 1.0);
}
