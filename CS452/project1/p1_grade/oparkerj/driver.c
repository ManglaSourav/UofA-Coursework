/*
 * Author: Parker Jones
 *
 * This driver program tests the library functions. This program draws a
 * wireframe cube on the screen that can be rotated and scaled, the color can
 * be changed, and instructions are shown for all key functions.
 */

#include "library.h"

#define WIDTH 640
#define HEIGHT 480

// Represents a rotation (quaternion)
typedef struct rotation rotation;
struct rotation
{
    double w;
    double x;
    double y;
    double z;
};

// Holds points and directions
typedef struct vec vec;
struct vec
{
    double x;
    double y;
    double z;
};

// Corners of a unit cube
const vec cube[8] = {
        {-0.5, -0.5, -0.5},
        {0.5, -0.5, -0.5},
        {0.5, 0.5, -0.5},
        {-0.5, 0.5, -0.5},
        {-0.5, -0.5, 0.5},
        {0.5, -0.5, 0.5},
        {0.5, 0.5, 0.5},
        {-0.5, 0.5, 0.5}
};

// Smaller dimension of the screen
const double screen_scale = WIDTH < HEIGHT ? WIDTH : HEIGHT;

// Pre-selected color choices
const color_t colors[8] = {
        COLOR255(255, 255, 255),
        COLOR255(255, 0, 0),
        COLOR255(255, 170, 0),
        COLOR255(255, 255, 0),
        COLOR255(0, 255, 0),
        COLOR255(0, 170, 255),
        COLOR255(0, 0, 255),
        COLOR255(255, 0, 255)
};

/*
 * Approximation of sin function.
 * This uses Bhaskara's approximation. However, the approximation is
 * only defined for 0-180 degrees, so extra steps are taken for values
 * outside that range.
 */
double sin(double x)
{
    double norm = x / 180;
    int sign = x < 0 ? (int) norm - 1 : (int) norm;
    sign = sign % 2 ? -1 : 1;
    if (x < 0) x += 180 * ((int) norm + 1);
    else if (x > 180) x -= 180 * ((int) norm);
    return sign * (x * 4 * (180 - x)) / (40500 - x * (180 - x));
}

/*
 * Approximation of cos function.
 * See docs for sin.
 */
double cos(double x)
{
    return sin(x + 90);
}

/*
 * Combine two rotations.
 * This produces the Hamilton product.
 * The result of this operation is rotation that rotates by A, then by B.
 */
rotation rot_combine(rotation a, rotation b)
{
    return (rotation) {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
}

/*
 * Rotate a vec using a rotation.
 */
vec rot_point(vec p, rotation r)
{
    rotation rp = {0, p.x, p.y, p.z};
    rp = rot_combine(r, rp);
    r.x = -r.x;
    r.y = -r.y;
    r.z = -r.z;
    rp = rot_combine(rp, r);
    return (vec) {rp.x, rp.y, rp.z};
}

/*
 * Create a rotation from an angle (in degrees) and
 * a unit vector (axis of rotation).
 */
rotation rot_angle(double angle, vec axis)
{
    double half = sin(angle / 2);
    return (rotation) {
        cos(angle / 2),
        half * axis.x,
        half * axis.y,
        half * axis.z
    };
}

/*
 * Return the inverse of a rotation.
 */
rotation rot_inverse(rotation r)
{
    double len = r.w * r.w + r.x * r.x + r.y * r.y + r.z * r.z;
    r.w /= len;
    r.x /= -len;
    r.y /= -len;
    r.z /= -len;
    return r;
}

/*
 * Draw a line from one point to another using Bresenham's line algorithm.
 */
void draw_line(int x1, int y1, int x2, int y2, color_t c)
{
    int dx = x2 - x1;
    int vx = dx >= 0 ? 1 : -1;
    if (dx < 0) dx = -dx;
    int dy = y2 - y1;
    int vy = dy >= 0 ? 1 : -1;
    if (dy > 0) dy = -dy;
    int error = dx + dy;
    for (;;)
    {
        draw_pixel(x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        int e = error * 2;
        if (e >= dy)
        {
            if (x1 == x2) break;
            error += dy;
            x1 += vx;
        }
        if (e <= dx)
        {
            if (y1 == y2) break;
            error += dx;
            y1 += vy;
        }
    }
}

/*
 * Draw a line between two points by projecting it onto the camera plane.
 */
void draw_line3d(vec a, vec b, color_t c)
{
    // Z-scaling
    a.x /= a.z;
    a.y /= -a.z;
    b.x /= b.z;
    b.y /= -b.z;
    // Scale to screen size and offset to center of screen
    a.x = a.x * screen_scale + WIDTH / 2.0;
    a.y = a.y * screen_scale + HEIGHT / 2.0;
    b.x = b.x * screen_scale + WIDTH / 2.0;
    b.y = b.y * screen_scale + HEIGHT / 2.0;
    draw_line((int) a.x, (int) a.y, (int) b.x, (int) b.y, c);
}

/*
 * Draw a 3d line with the given offset, scale, and rotation.
 */
void draw_line3d_transform(vec a, vec b, vec offset, vec scale, rotation rotation, color_t color)
{
    a = rot_point(a, rotation);
    b = rot_point(b, rotation);
    a = (vec) {a.x * scale.x + offset.x, a.y * scale.y + offset.y, a.z * scale.z + offset.z};
    b = (vec) {b.x * scale.x + offset.x, b.y * scale.y + offset.y, b.z * scale.z + offset.z};
    draw_line3d(a, b, color);

}

/*
 * Draw a cube with the given center, scale, and rotation.
 */
void draw_wirecube(vec center, vec scale, rotation rotation, color_t color)
{
    for (int i = 0; i < 4; i++)
    {
        draw_line3d_transform(cube[i], cube[(i + 1) % 4], center, scale, rotation, color);
    }
    for (int i = 0; i < 4; i++)
    {
        draw_line3d_transform(cube[i + 4], cube[(i + 1) % 4 + 4], center, scale, rotation, color);
    }
    for (int i = 0; i < 4; i++)
    {
        draw_line3d_transform(cube[i], cube[i + 4], center, scale, rotation, color);
    }
}

/*
 * Print text on the screen with a black background that shows available keys.
 */
void draw_instructions()
{
    color_t white = COLOR255(255, 255, 255);
    // White outline
    draw_rect(6, 6, 8 * 19 + 8, 16 * 4 + 8, white);

    draw_text(10, 10, "W/A/S/D/Q/E: Rotate", white);
    draw_text(10, 10 + 16, "R/F: Scale", white);
    draw_text(10, 10 + 16 * 2, "C: Color", white);
    draw_text(10, 10 + 16 * 3, "0: Exit", white);
}

int main()
{
    init_graphics();

    // Rotation directions
    rotation up = rot_angle(2, (vec) {1, 0, 0});
    rotation down = rot_inverse(up);
    rotation left = rot_angle(2, (vec) {0, 1, 0});
    rotation right = rot_inverse(left);
    rotation rollLeft = rot_angle(2, (vec) {0, 0, 1});
    rotation rollRight = rot_inverse(rollLeft);

    rotation rot = {1, 0, 0, 0};
    double scale = 0.5;
    int color = 0;
    int update = 1;

    for (;;)
    {
        char key = getkey();
        if (!key && !update)
        {
            sleep_ms(20);
            continue;
        }
        update = 0;
        if (key == '0') break;

        switch (key)
        {
            case 'c':
                color++;
                if (color == 8) color = 0;
                break;
            case 'r':
                scale += 0.05;
                if (scale > 1.5) scale = 1.5;
                break;
            case 'f':
                scale -= 0.05;
                if (scale < 0.2) scale = 0.2;
                break;
            case 'w':
                rot = rot_combine(up, rot);
                break;
            case 'a':
                rot = rot_combine(left, rot);
                break;
            case 's':
                rot = rot_combine(down, rot);
                break;
            case 'd':
                rot = rot_combine(right, rot);
                break;
            case 'q':
                rot = rot_combine(rollLeft, rot);
                break;
            case 'e':
                rot = rot_combine(rollRight, rot);
                break;
        }

        clear_screen();
        draw_wirecube((vec) {0, 0, 2}, (vec) {scale, scale, scale}, rot, colors[color]);
        draw_instructions();
        sleep_ms(20);
    }

    clear_screen();
    exit_graphics();
    return 0;
}