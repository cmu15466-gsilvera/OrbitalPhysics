#pragma once

#include <glm/glm.hpp>

#include <string>

namespace linalg {
    //vec3
    struct vec3 {
        vec3(long double n) : x(n), y(n), z(n) {}
        vec3(long double x_, long double y_, long double z_) : x(x_), y(y_), z(z_) {}
        vec3(glm::vec3 v) : x(v.x), y(v.y), z(v.z) {}
        long double x;
        long double y;
        long double z;
    };

    //vector-vector operators
    vec3 operator-(vec3 a) {
        return vec3(-a.x, -a.y, -a.z);
    }
    vec3 operator+(vec3 a, vec3 b) {
        return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    vec3 operator-(vec3 a, vec3 b) {
        return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    //vector-scalar operators
    vec3 operator+(vec3 a, long double f) {
        return vec3(a.x + f, a.y + f, a.z + f);
    }
    vec3 operator+(long double f, vec3 a) {
        return vec3(f + a.x, f + a.y, f + a.z);
    }
    vec3 operator-(vec3 a, long double f) {
        return vec3(a.x - f, a.y - f, a.z - f);
    }
    vec3 operator-(long double f, vec3 a) {
        return vec3(f - a.x, f - a.y, f - a.z);
    }
    vec3 operator*(vec3 a, long double f) {
        return vec3(a.x * f, a.y * f, a.z * f);
    }
    vec3 operator*(long double f, vec3 a) {
        return vec3(f * a.x, f * a.y, f * a.z);
    }
    vec3 operator/(vec3 a, long double f) {
        return vec3(a.x / f, a.y / f, a.z / f);
    }

    long double norm_sq(vec3 a) {
        return a.x * a.x + a.y * a.y + a.z * a.z;
    }
    long double norm(vec3 a) {
        return std::sqrt(norm_sq(a));
    }
    vec3 normalize (vec3 a) {
        return a / norm(a);
    }
    vec3 cross(vec3 a, vec3 b);

    //conversion to glm
    glm::vec3 to_glm(vec3 a) {
        return glm::vec3(
            static_cast< float >(a.x),
            static_cast< float >(a.y),
            static_cast< float >(a.z)
        );
    }

    //printing
    std::string to_string(vec3 a) {
        return "(" + std::to_string(a.x) + ", " + std::to_string(a.y) + ", " + std::to_string(a.z) + ")";
    }
}
