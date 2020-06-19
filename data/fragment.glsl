#version 450 core

in vec3 frag_pos;
in vec3 frag_norm;

out vec4 color;

uniform vec3 camera_pos;

void main() {
    vec3 light_dir = vec3(-1.0, -1.0, -1.0);
    vec3 light_color = vec3(1.0, 1.0, 1.0);
    vec3 obj_color = vec3(1.0, 0.5, 0.0);

    float ambient_strength = 0.2;
    vec3 ambient = ambient_strength * light_color;

    vec3 norm = normalize(frag_norm);
    vec3 frag_to_light_dir = normalize(-light_dir);

    float diffuse_fac = max(dot(norm, frag_to_light_dir), 0.0);
    vec3 diffuse = diffuse_fac * light_color;

    float specular_strength = 0.5;
    vec3 camera_dir = normalize(camera_pos - frag_pos);
    vec3 reflect_dir = reflect(-frag_to_light_dir, norm);
    float specular_fac = pow(max(dot(camera_dir, reflect_dir), 0.0), 32);
    vec3 specular = specular_strength * specular_fac * light_color;

    color = vec4((ambient + diffuse + specular) * obj_color, 1.0);
}
