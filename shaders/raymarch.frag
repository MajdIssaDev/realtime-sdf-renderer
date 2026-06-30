#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform float u_time;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraFront;
uniform vec3 u_cameraUp;
uniform vec2 u_resolution;

float sdSphere(vec3 p, float radius)
{
    return length(p) - radius;
}

float mapScene(vec3 p)
{
    return sdSphere(p - vec3(0.0, 0.0, -5.0), 1.5);
}

void main()
{
    const float focalLength = 1.5;

    vec3 ro = u_cameraPos;
    vec3 forward = normalize(u_cameraFront);
    vec3 right = normalize(cross(forward, u_cameraUp));
    vec3 up = normalize(cross(right, forward));

    vec2 pixelUV = gl_FragCoord.xy / u_resolution;
    vec2 ndc = pixelUV * 2.0 - 1.0;
    ndc.x *= u_resolution.x / u_resolution.y;

    vec3 rd = normalize(ndc.x * right + ndc.y * up + focalLength * forward);

    float t = 0.0;
    bool hit = false;

    for (int i = 0; i < 64; ++i)
    {
        vec3 p = ro + rd * t;
        float d = mapScene(p);

        if (d < 0.001)
        {
            hit = true;
            break;
        }

        t += d;
        if (t > 100.0)
        {
            break;
        }
    }

    if (hit)
    {
        FragColor = vec4(0.85, 0.35, 0.25, 1.0);
    }
    else
    {
        FragColor = vec4(0.08, 0.09, 0.14, 1.0);
    }
}
