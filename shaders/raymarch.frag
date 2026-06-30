#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform float u_time;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraFront;
uniform vec3 u_cameraUp;
uniform vec2 u_resolution;

uniform vec3  u_sphereColor;
uniform float u_sphereRoughness;
uniform float u_sphereSpecular;
uniform float u_sphereMetalness;

uniform vec3  u_boxColor;
uniform float u_boxRoughness;
uniform float u_boxSpecular;
uniform float u_boxMetalness;

uniform int u_selectedObjectID;

struct Surface {
    float dist;
    float matID;
    float blendH;
};

struct MaterialProps {
    vec3  baseColor;
    float ambient;
    float diffuse;
    float specular;
    float shininess;
};

const float HIT_EPS      = 0.0005;
const float SMIN_BLEND_K = 0.5;

const float MAT_SPHERE = 1.0;
const float MAT_BOX    = 2.0;
const float MAT_FLOOR  = 3.0;

const vec3 kSkyColor   = vec3(0.08, 0.10, 0.16);
const vec3 kLightDir   = normalize(vec3(-0.6, 1.0, 0.4));
const vec3 kLightColor = vec3(1.0);

const float SHADOW_BIAS = 0.02;

float sdBox(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdPlane(vec3 p, float h)
{
    return p.y - h;
}

vec2 sminWithFactor(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    float d = mix(b, a, h) - k * h * (1.0 - h);
    return vec2(d, h);
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

Surface mapObjectSurface(vec3 p)
{
    float boxDist, sphereDist, floorDist;
    sceneDistances(p, boxDist, sphereDist, floorDist);

    vec2 blended = sminWithFactor(boxDist, sphereDist, SMIN_BLEND_K);
    float matID = (boxDist < sphereDist) ? MAT_BOX : MAT_SPHERE;

    return Surface(blended.x, matID, blended.y);
}

Surface map(vec3 p)
{
    Surface objectSurf = mapObjectSurface(p);
    Surface floorSurf  = Surface(sdPlane(p, -1.0), MAT_FLOOR, 0.0);

    return (objectSurf.dist < floorSurf.dist) ? objectSurf : floorSurf;
}

vec3 getNormal(vec3 p)
{
    const float h = 0.0005;
    const vec2 k = vec2(1.0, -1.0);
    return normalize(
        k.xyy * map(p + k.xyy * h).dist +
        k.yyx * map(p + k.yyx * h).dist +
        k.yxy * map(p + k.yxy * h).dist +
        k.xxx * map(p + k.xxx * h).dist
    );
}

float getSoftShadow(vec3 ro, vec3 rd)
{
    float res = 1.0;
    float t = 0.05;

    for (int i = 0; i < 64; ++i)
    {
        vec3 p = ro + rd * t;
        float h = mapObjectSurface(p).dist;

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

float getAO(vec3 p, vec3 n)
{
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 0; i < 5; i++)
    {
        float h = 0.02 + 0.12 * float(i) / 4.0;
        vec3 samplePos = p + n * h;
        float d = map(samplePos).dist;

        occ += (h - d) * sca;
        sca *= 0.95;
    }

    return clamp(1.0 - 2.0 * occ, 0.0, 1.0);
}

MaterialProps getMaterial(float matID, vec3 p, float blendH)
{
    if (matID == MAT_FLOOR)
    {
        bool cx = fract(p.x) > 0.5;
        bool cz = fract(p.z) > 0.5;
        vec3 checker = (cx != cz) ? vec3(0.95) : vec3(0.05);
        return MaterialProps(checker, 0.20, 0.70, 0.10, 32.0);
    }

    vec3 activeColor      = mix(u_sphereColor, u_boxColor, blendH);
    float activeRoughness = mix(u_sphereRoughness, u_boxRoughness, blendH);
    float activeSpecular  = mix(u_sphereSpecular,  u_boxSpecular,  blendH);
    float activeMetalness = mix(u_sphereMetalness, u_boxMetalness, blendH);

    float shininess = mix(1.0, 128.0, 1.0 - activeRoughness);
    float diffuseStrength  = mix(0.85, 0.05, activeMetalness);
    float ambientStrength  = mix(0.15, 0.10 + 0.35 * activeMetalness, activeMetalness);
    float specularStrength = activeSpecular * (1.0 + activeMetalness);

    return MaterialProps(activeColor, ambientStrength, diffuseStrength, specularStrength, shininess);
}

vec3 blinnPhong(vec3 normal, vec3 viewDir, MaterialProps mat, float shadow, float ao)
{
    vec3 N = normalize(normal);
    vec3 L = kLightDir;
    vec3 V = normalize(viewDir);
    vec3 H = normalize(L + V);

    float diffuse = max(dot(N, L), 0.0);
    float spec    = pow(max(dot(N, H), 0.0), mat.shininess);

    vec3 ambient  = mat.ambient  * kLightColor * ao;
    vec3 diffuseC = mat.diffuse  * diffuse * kLightColor * shadow * mix(0.75, 1.0, ao);
    vec3 specular = mat.specular * spec    * kLightColor * shadow;

    return clamp(mat.baseColor * (ambient + diffuseC + specular), 0.0, 1.0);
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
        float d = map(p).dist;

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
        Surface hitSurf = map(hitPos);
        vec3 normal = getNormal(hitPos);
        vec3 shadowOrigin = hitPos + normal * SHADOW_BIAS;
        float shadow = getSoftShadow(shadowOrigin, kLightDir);
        float ao = getAO(hitPos, normal);
        MaterialProps mat = getMaterial(hitSurf.matID, hitPos, hitSurf.blendH);
        vec3 finalColor = blinnPhong(normal, -rd, mat, shadow, ao);

        if (u_selectedObjectID != 0 && hitSurf.matID != MAT_FLOOR)
        {
            float boxDist, sphereDist, floorDist;
            sceneDistances(hitPos, boxDist, sphereDist, floorDist);

            bool belongsToSelection = false;
            if (u_selectedObjectID == 1)
            {
                belongsToSelection = sphereDist <= boxDist;
            }
            else if (u_selectedObjectID == 2)
            {
                belongsToSelection = boxDist <= sphereDist;
            }

            if (belongsToSelection)
            {
                float fresnel = pow(1.0 - max(dot(normal, -rd), 0.0), 2.5);
                vec3 highlightColor = vec3(1.0, 0.92, 0.35);
                float pulse = 0.55 + 0.45 * sin(u_time * 4.0);
                finalColor = mix(finalColor, highlightColor, fresnel * 0.35 * pulse);
                finalColor += highlightColor * fresnel * 0.12 * pulse;
            }
        }

        const float fogDensity = 0.035;
        float fogFactor = clamp(1.0 - exp(-fogDensity * t), 0.0, 1.0);
        finalColor = mix(finalColor, kSkyColor, fogFactor);

        FragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
    }
    else
    {
        float sky = clamp(0.5 + 0.5 * rd.y, 0.0, 1.0);
        vec3 bg = mix(kSkyColor * 0.75, kSkyColor, sky);
        FragColor = vec4(bg, 1.0);
    }
}
