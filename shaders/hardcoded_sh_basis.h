// took from https://github.com/google/spherical-harmonics/blob/master/sh/spherical_harmonics.cc

float HardcodedSH00(const vec3 d)
{
    return 0.282095;
}

float HardcodedSH1n1(const vec3 d)
{
    return -0.488603 * d.y;
}

float HardcodedSH10(const vec3 d)
{
    return 0.488603 * d.z;
}

float HardcodedSH1p1(const vec3 d)
{
    return -0.488603 * d.x;
}

float HardcodedSH2n2(const vec3 d)
{
    return 1.092548 * d.x * d.y;
}

float HardcodedSH2n1(const vec3 d)
{
    return -1.092548 * d.y * d.z;
}

float HardcodedSH20(const vec3 d)
{
    return 0.315392 * (-d.x * d.x - d.y * d.y + 2.0 * d.z * d.z);
}

float HardcodedSH2p1(const vec3 d)
{
    return -1.092548 * d.x * d.z;
}

float HardcodedSH2p2(const vec3 d)
{
    return 0.546274 * (d.x * d.x - d.y * d.y);
}

float HardcodedSH3n3(const vec3 d)
{
    return -0.590044 * d.y * (3.0 * d.x * d.x - d.y * d.y);
}

float HardcodedSH3n2(const vec3 d)
{
    return 2.890611 * d.x * d.y * d.z;
}

float HardcodedSH3n1(const vec3 d)
{
    return -0.457046 * d.y * (4.0 * d.z * d.z - d.x * d.x - d.y * d.y);
}

float HardcodedSH30(const vec3 d)
{
    return 0.373176 * d.z * (2.0 * d.z * d.z - 3.0 * d.x * d.x - 3.0 * d.y * d.y);
}

float HardcodedSH3p1(const vec3 d)
{
    return -0.457046 * d.x * (4.0 * d.z * d.z - d.x * d.x - d.y * d.y);
}

float HardcodedSH3p2(const vec3 d)
{
    return 1.445306 * d.z * (d.x * d.x - d.y * d.y);
}

float HardcodedSH3p3(const vec3 d)
{
    return -0.590044 * d.x * (d.x * d.x - 3.0 * d.y * d.y);
}

float HardcodedSH4n4(const vec3 d)
{
    return 2.503343 * d.x * d.y * (d.x * d.x - d.y * d.y);
}

float HardcodedSH4n3(const vec3 d)
{
    return -1.770131 * d.y * d.z * (3.0 * d.x * d.x - d.y * d.y);
}

float HardcodedSH4n2(const vec3 d)
{
    return 0.946175 * d.x * d.y * (7.0 * d.z * d.z - 1.0);
}

float HardcodedSH4n1(const vec3 d)
{
    return -0.669047 * d.y * d.z * (7.0 * d.z * d.z - 3.0);
}

float HardcodedSH40(const vec3 d)
{
    float z2 = d.z * d.z;
    return 0.105786 * (35.0 * z2 * z2 - 30.0 * z2 + 3.0);
}

float HardcodedSH4p1(const vec3 d)
{
    return -0.669047 * d.x * d.z * (7.0 * d.z * d.z - 3.0);
}

float HardcodedSH4p2(const vec3 d)
{
    return 0.473087 * (d.x * d.x - d.y * d.y) * (7.0 * d.z * d.z - 1.0);
}

float HardcodedSH4p3(const vec3 d)
{
    return -1.770131 * d.x * d.z * (d.x * d.x - 3.0 * d.y * d.y);
}

float HardcodedSH4p4(const vec3 d)
{
    float x2 = d.x * d.x;
    float y2 = d.y * d.y;
    return 0.625836 * (x2 * (x2 - 3.0 * y2) - y2 * (3.0 * x2 - y2));
}

float SH(int l, int m, const vec3 dir)
{
    switch (l)
    {
        case 0:
            return HardcodedSH00(dir);
        case 1:
            switch (m) {
                case -1:
                    return HardcodedSH1n1(dir);
                case 0:
                    return HardcodedSH10(dir);
                case 1:
                    return HardcodedSH1p1(dir);
            }
        case 2:
            switch (m) {
                case -2:
                    return HardcodedSH2n2(dir);
                case -1:
                    return HardcodedSH2n1(dir);
                case 0:
                    return HardcodedSH20(dir);
                case 1:
                    return HardcodedSH2p1(dir);
                case 2:
                    return HardcodedSH2p2(dir);
            }
        case 3:
            switch (m) {
                case -3:
                    return HardcodedSH3n3(dir);
                case -2:
                    return HardcodedSH3n2(dir);
                case -1:
                    return HardcodedSH3n1(dir);
                case 0:
                    return HardcodedSH30(dir);
                case 1:
                    return HardcodedSH3p1(dir);
                case 2:
                    return HardcodedSH3p2(dir);
                case 3:
                    return HardcodedSH3p3(dir);
            }
        case 4:
            switch (m) {
                case -4:
                    return HardcodedSH4n4(dir);
                case -3:
                    return HardcodedSH4n3(dir);
                case -2:
                    return HardcodedSH4n2(dir);
                case -1:
                    return HardcodedSH4n1(dir);
                case 0:
                    return HardcodedSH40(dir);
                case 1:
                    return HardcodedSH4p1(dir);
                case 2:
                    return HardcodedSH4p2(dir);
                case 3:
                    return HardcodedSH4p3(dir);
                case 4:
                    return HardcodedSH4p4(dir);
            }
    }

    return 0.0;
}

