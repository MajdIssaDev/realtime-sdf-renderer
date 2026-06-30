#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform float u_time;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraFront;
uniform vec3 u_cameraUp;
uniform vec2 u_resolution;

const float HIT_EPS       = 0.001;
const float NORMAL_EPS    = 0.001;

const vec3  kMaterialColor = vec3(0.85, 0.35, 0.25);
const vec3  kLightDir      = normalize(vec3(1.0, 1.0, -1.0));
const vec3  kLightColor    = vec3(1.0);

const float kAmbientStrength  = 0.15;
const float kDiffuseStrength  = 0.75;
const float kSpecularStrength = 0.35;
const float kShininess        = 64.0;

float sdSphere(vec3 p, float radius)
{
    return length(p) - radius;
}

float mapScene(vec3 p)
{
    return sdSphere(p - vec3(0.0, 0.0, -5.0), 1.5);
}

vec3 getNormal(vec3 p)
{
    const float e = NORMAL_EPS;

    float dx = mapScene(p + vec3(e, 0.0, 0.0)) - mapScene(p - vec3(e, 0.0, 0.0));
    float dy = mapScene(p + vec3(0.0, e, 0.0)) - mapScene(p - vec3(0.0, e, 0.0));
    float dz = mapScene(p + vec3(0.0, 0.0, e)) - mapScene(p - vec3(0.0, 0.0, e));

    return normalize(vec3(dx, dy, dz));
}

vec3 blinnPhong(vec3 normal, vec3 viewDir, vec3 baseColor)
{
    vec3 N = normalize(normal);
    vec3 L = kLightDir;
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    float diffuse = max(dot(N, L), 0.0);
    float spec    = pow(max(dot(N, H), 0.0), kShininess);

    vec3 ambient  = kAmbientStrength  * kLightColor;
    vec3 diffuseC = kDiffuseStrength  * diffuse * kLightColor;
    vec3 specular = kSpecularStrength * spec    * kLightColor;

    vec3 intensity = ambient + diffuseC + specular;
    return clamp(baseColor * intensity, 0.0, 1.0);
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

        if (d < HIT_EPS)
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
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos);
        vec3 shaded = blinnPhong(normal, -rd, kMaterialColor);
        FragColor = vec4(shaded, 1.0);
    }
    else
    {
        FragColor = vec4(0.08, 0.09, 0.14, 1.0);
    }
}
