#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform float u_time;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraFront;
uniform vec3 u_cameraUp;
uniform vec2 u_resolution;

const float HIT_EPS            = 0.0005;
const float SMIN_BLEND_K       = 0.5;

const vec3  kMaterialColor = vec3(0.85, 0.35, 0.25);
const vec3  kLightDir      = normalize(vec3(-0.6, 1.0, 0.4));
const vec3  kLightColor    = vec3(1.0);

const float kAmbientStrength  = 0.15;
const float kDiffuseStrength  = 0.75;
const float kSpecularStrength = 0.35;
const float kShininess        = 64.0;

const float SHADOW_BIAS       = 0.02;

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdPlane(vec3 p, float h)
{
    return p.y - h;
}

float smin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

void sceneDistances(vec3 p, out float boxDist, out float sphereDist, out float floorDist)
{
    boxDist = sdBox(p, vec3(1.0));

    vec3 sphereCenter = vec3(
        sin(u_time) * 2.0,
        0.5 + 0.3 * cos(u_time * 1.5),
        cos(u_time) * 2.0
    );

    sphereDist = length(p - sphereCenter) - 1.0;
    floorDist  = sdPlane(p, -1.0);
}

float mapObject(vec3 p)
{
    float boxDist, sphereDist, floorDist;
    sceneDistances(p, boxDist, sphereDist, floorDist);
    return smin(boxDist, sphereDist, SMIN_BLEND_K);
}

float mapScene(vec3 p)
{
    float boxDist, sphereDist, floorDist;
    sceneDistances(p, boxDist, sphereDist, floorDist);
    return min(smin(boxDist, sphereDist, SMIN_BLEND_K), floorDist);
}

vec3 getNormal(vec3 p)
{
    const float h = 0.0005;
    const vec2 k = vec2(1.0, -1.0);
    return normalize(
        k.xyy * mapScene(p + k.xyy * h) +
        k.yyx * mapScene(p + k.yyx * h) +
        k.yxy * mapScene(p + k.yxy * h) +
        k.xxx * mapScene(p + k.xxx * h)
    );
}

float getSoftShadow(vec3 ro, vec3 rd)
{
    float res = 1.0;
    float t = 0.05;

    for (int i = 0; i < 64; ++i)
    {
        vec3 p = ro + rd * t;
        float h = mapObject(p);

        if (h < 0.001)
        {
            return 0.0;
        }

        res = min(res, 16.0 * h / t);
        t += clamp(h, 0.02, 0.25);

        if (t > 20.0)
        {
            break;
        }
    }

    return clamp(res, 0.0, 1.0);
}

vec3 blinnPhong(vec3 normal, vec3 viewDir, vec3 baseColor, float shadow)
{
    vec3 N = normalize(normal);
    vec3 L = kLightDir;
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    float diffuse = max(dot(N, L), 0.0);
    float spec    = pow(max(dot(N, H), 0.0), kShininess);

    vec3 ambient  = kAmbientStrength  * kLightColor;
    vec3 diffuseC = kDiffuseStrength  * diffuse * kLightColor * shadow;
    vec3 specular = kSpecularStrength * spec    * kLightColor * shadow;

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

    for (int i = 0; i < 100; ++i)
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
        vec3 hitPos = ro + rd * max(t - HIT_EPS, 0.0);
        vec3 normal = getNormal(hitPos);
        vec3 shadowOrigin = hitPos + normal * SHADOW_BIAS;
        float shadow = getSoftShadow(shadowOrigin, kLightDir);
        vec3 finalColor = blinnPhong(normal, -rd, kMaterialColor, shadow);
        FragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
    }
    else
    {
        FragColor = vec4(0.08, 0.09, 0.14, 1.0);
    }
}
