#version 330 core

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightPos;   // 포인트 라이트 위치

out vec4 FragColor;

void main()
{
    float dist = length(FragPos - lightPos);

    // 거리 기반 감쇠
    float attenuation = 1.0 / (1.0 + 0.4 * dist + 0.6 * dist * dist);

    // 약간의 주변광
    vec3 ambient = objectColor * 0.15;

    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 norm = normalize(Normal);
    vec3 lightColor = vec3(1.0);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * objectColor;

    vec3 color = (ambient + diffuse) * attenuation;
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
}
