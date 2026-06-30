#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform float u_time;

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
    const float aspect = 16.0 / 9.0;

    vec2 ndc = uv * 2.0 - 1.0;
    ndc.x *= aspect;

    vec3 ro = vec3(0.0, 0.0, 0.0);
    vec3 rd = normalize(vec3(ndc, -1.0));

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
