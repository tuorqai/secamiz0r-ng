/**
 * Copyright (c) 2024 tuorqai
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/**
 * secamiz0r.c: SECAM fire frei0r plugin.
 */

#include <math.h>
#include <stdlib.h>
#include "frei0r.h"

/**
 * Limit integer value to the range.
 */
static int clamp_int(int value, int a, int b)
{
    return (value < a) ? a : ((value > b) ? b : value);
}

/**
 * Limit value to the 8-bit range.
 */
static uint8_t clamp_byte(int value)
{
    return (uint8_t) clamp_int(value, 0, 255);
}

/**
 * Convert 24-bit RGB value to array of floating-point values.
 */
static void unpack_rgb(float *rgb, uint8_t const *src)
{
    rgb[0] = ((float) src[0]) / 255.f;
    rgb[1] = ((float) src[1]) / 255.f;
    rgb[2] = ((float) src[2]) / 255.f;
}

/**
 * Calculate 8-bit luminance value from floating-point RGB value.
 */
static uint8_t y_from_rgb(float const *rgb)
{
    return (uint8_t) (16.0 + (65.7380 * rgb[0]) + (129.057 * rgb[1]) + (25.0640 * rgb[2]));
}

/**
 * Calculate 8-bit B-Y value from floating-point RGB value.
 */
static uint8_t u_from_rgb(float const *rgb)
{
    return (uint8_t) (128.0 - (37.9450 * rgb[0]) - (74.4940 * rgb[1]) + (112.439 * rgb[2]));
}

/**
 * Calculate 8-bit R-Y value from floating-point RGB value.
 */
static uint8_t v_from_rgb(float const *rgb)
{
    return (uint8_t) (128.0 + (112.439 * rgb[0]) - (94.1540 * rgb[1]) - (18.2850 * rgb[2]));
}

/**
 * Convert floating-point YUV value to 24-bit RGB.
 */
static void rgb_from_yuv(uint8_t *dst, float y, float u, float v)
{
    dst[0] = clamp_byte((int) ((298.082 * y) + (408.583 * v) - 222.921));
    dst[1] = clamp_byte((int) ((298.082 * y) - (100.291 * u) - (208.120 * v) + 135.576));
    dst[2] = clamp_byte((int) ((298.082 * y) + (516.412 * u) - 276.836));
}

/**
 * Unsigned modulo.
 */
static unsigned int umod(int a, int b)
{
    return ((a % b) + b) % b;
}

/**
 * Do I have any idea what this does?
 * Make random integer out of random integer.
 */
static int juice(int j)
{
    j ^= j << 13;
    j ^= j >> 17;
    j ^= j << 5;

    return j;
}

/**
 * secamiz0r instance struct.
 */
struct secamiz0r
{
    unsigned int width;
    unsigned int height;
    size_t frame_count;

    double fire_intensity;
    int fire_threshold;
    int fire_seed;

    double noise_intensity;
    int luma_noise;
    int chroma_noise;
    int echo_offset;
};

/**
 * Some values are dependent on "fire intensity" parameter.
 */
static void set_fire_intensity(struct secamiz0r *self, double fire_intensity)
{
    double const x = self->fire_intensity = fire_intensity;

    self->fire_threshold = 1024 - (int) (x * x * 256.0);
    self->fire_seed = (int) (x * 1024.0);
}

/**
* Some values are dependent on "noise intensity" parameter.
*/
static void set_noise_intensity(struct secamiz0r *self, double noise_intensity)
{
    double const x = self->noise_intensity = noise_intensity;

    self->luma_noise = clamp_int((int) (x * x * 256.0), 16, 224);
    self->chroma_noise = clamp_int((int) (x * 256.0), 32, 256);
    self->echo_offset = clamp_int((int) (x * 8.0), 2, 16);
}

/**
 * frei0r plugin entry point: seems to be deprecated.
 */
int f0r_init()
{
    return 1;
}

/**
 * Same as f0r_init(), no signs of usage.
 */
void f0r_deinit()
{
}

/**
 * Expose some metadata.
 */
void f0r_get_plugin_info(f0r_plugin_info_t *info)
{
    info->name = "secamiz0r";
    info->author = "tuorqai";
    info->plugin_type = F0R_PLUGIN_TYPE_FILTER;
    info->color_model = F0R_COLOR_MODEL_RGBA8888;
    info->frei0r_version = FREI0R_MAJOR_VERSION;
    info->major_version = 2;
    info->minor_version = 0;
    info->num_params = 2;
    info->explanation = "SECAM Fire effect";
}

/**
 * Expose parameter metadata.
 * Note: kdenlive uses .name field to refer to these.
 */
void f0r_get_param_info(f0r_param_info_t *info, int index)
{
    switch (index) {
    case 0:
        info->name = "Fire intensity";
        info->explanation = NULL;
        info->type = F0R_PARAM_DOUBLE;
        break;
    case 1:
        info->name = "Noise intensity";
        info->explanation = NULL;
        info->type = F0R_PARAM_DOUBLE;
        break;
    default:
        break;
    }
}

/**
 * The actual entry point of a frei0r plugin.
 */
f0r_instance_t f0r_construct(unsigned int width, unsigned int height)
{
    struct secamiz0r *self = malloc(sizeof(*self));

    if (!self) {
        return NULL;
    }

    self->width = width;
    self->height = height;
    self->frame_count = 0;

    set_fire_intensity(self, 0.125);
    set_noise_intensity(self, 0.125);

    return self;
}

/**
 * Don't forget to turn off your TV.
 */
void f0r_destruct(f0r_instance_t instance)
{
    free(instance);
}

/**
 * Parameter value setter.
 */
void f0r_set_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *self = instance;

    switch (index) {
    case 0:
        set_fire_intensity(self, *((double const *) param));
        break;
    case 1:
        set_noise_intensity(self, *((double const *) param));
        break;
    default:
        break;
    }
}

/**
 * Parameter value getter.
 */
void f0r_get_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *self = instance;

    switch (index) {
    case 0:
        *((double *) param) = self->fire_intensity;
        break;
    case 1:
        *((double *) param) = self->noise_intensity;
        break;
    default:
        break;
    }
}

/**
 * Filtering Stage 1. Copy two consecutive pixel rows from source buffer to the destination
 * buffer, converting it to YUV on the fly.
 * We need to do this, because this plugin aims not to allocate memory at all (with the
 * exception of secamiz0r struct in f0r_construct()). So the only available storage for us
 * is the destination buffer provided by frei0r itself.
 */
static void copy_pair_as_yuv(struct secamiz0r *self, uint8_t *dst_even, uint8_t *dst_odd, uint8_t const *src_even, uint8_t const *src_odd)
{
    for (size_t i = 0; i < self->width; i += 2) {
        float rgb0_even[3];
        float rgb1_even[3];
        float rgb0_odd[3];
        float rgb1_odd[3];

        unpack_rgb(rgb0_even, &src_even[(i + 0) * 4]);
        unpack_rgb(rgb1_even, &src_even[(i + 1) * 4]);
        unpack_rgb(rgb0_odd, &src_odd[(i + 0) * 4]);
        unpack_rgb(rgb1_odd, &src_odd[(i + 1) * 4]);

        float rgb_even[] = {
            (rgb0_even[0] + rgb1_even[0]) / 2.f,
            (rgb0_even[1] + rgb1_even[1]) / 2.f,
            (rgb0_even[2] + rgb1_even[2]) / 2.f,
        };

        float rgb_odd[] = {
            (rgb0_odd[0] + rgb1_odd[0]) / 2.f,
            (rgb0_odd[1] + rgb1_odd[1]) / 2.f,
            (rgb0_odd[2] + rgb1_odd[2]) / 2.f,
        };

        uint8_t y0_even = y_from_rgb(rgb0_even);
        uint8_t y1_even = y_from_rgb(rgb1_even);
        uint8_t y0_odd = y_from_rgb(rgb0_odd);
        uint8_t y1_odd = y_from_rgb(rgb1_odd);
        uint8_t u = u_from_rgb(rgb_odd);
        uint8_t v = v_from_rgb(rgb_even);

        dst_even[(i + 0) * 4 + 0] = y0_even;
        dst_even[(i + 0) * 4 + 1] = v;
        dst_even[(i + 0) * 4 + 2] = 0;
        dst_even[(i + 0) * 4 + 3] = src_even[(i + 0) * 4 + 3];

        dst_even[(i + 1) * 4 + 0] = y1_even;
        dst_even[(i + 1) * 4 + 1] = v;
        dst_even[(i + 1) * 4 + 2] = 0;
        dst_even[(i + 1) * 4 + 3] = src_even[(i + 1) * 4 + 3];

        dst_odd[(i + 0) * 4 + 0] = y0_odd;
        dst_odd[(i + 0) * 4 + 1] = u;
        dst_odd[(i + 0) * 4 + 2] = 0;
        dst_odd[(i + 0) * 4 + 3] = src_odd[(i + 0) * 4 + 3];

        dst_odd[(i + 1) * 4 + 0] = y1_odd;
        dst_odd[(i + 1) * 4 + 1] = u;
        dst_odd[(i + 1) * 4 + 2] = 0;
        dst_odd[(i + 1) * 4 + 3] = src_odd[(i + 1) * 4 + 3];
    }
}

/**
 * Filtering Stage 2.1. The loop inside this function works on two consecutive
 * lines copied in Stage 1. It aims to detect areas where luminance level is
 * changed rapidly, and then marks those areas. To achieve this, we keep summing
 * up difference between two pixels, halving the sum every step (it kinda fades
 * away). To make this more chaotic, we also add a random value. Then, if at
 * some point the sum is larger than a threshold value, we mark this pixel.
 */
static void prefilter_pair(struct secamiz0r *self, uint8_t *even, uint8_t *odd)
{
    int r_even = rand();
    int r_odd = rand();

    int y_even_oscillation = self->fire_seed ? umod(r_even, self->fire_seed) : 0;
    int y_odd_oscillation = self->fire_seed ? umod(r_odd, self->fire_seed) : 0;

    for (size_t i = 1; i < self->width; i++) {
        y_even_oscillation += abs(even[i * 4 + 0] - even[i * 4 - 4] - umod(r_even, 512));
        y_odd_oscillation += abs(odd[i * 4 + 0] - odd[i * 4 - 4] - umod(r_odd, 512));

        if (y_even_oscillation > self->fire_threshold) {
            even[i * 4 + 2] = umod(r_even, 80);
        }

        if (y_odd_oscillation > self->fire_threshold) {
            odd[i * 4 + 2] = umod(r_odd, 80);
        }

        r_even = juice(r_even);
        r_odd = juice(r_odd);

        y_even_oscillation /= 2;
        y_odd_oscillation /= 2;
    }
}

/**
 * Filtering Stage 2.2. This actually modifies the image, adding random noise
 * and fires at marked areas.
 */
static void filter_pair(struct secamiz0r *self, uint8_t *even, uint8_t *odd)
{
    int r_even = rand();
    int r_odd = rand();

    int u_fire = 0;
    int u_fire_sign = 1;

    int v_fire = 0;
    int v_fire_sign = 1;

    int const fire_fade = 1;

    for (size_t i = 0; i < self->width; i++) {
        int y_even = even[i * 4 + 0];
        int y_odd = odd[i * 4 + 0];

        int u = (int) odd[i * 4 + 1] - 128;
        int v = (int) even[i * 4 + 1] - 128;

        int z_even = even[i * 4 + 2];
        int z_odd = odd[i * 4 + 2];

        if (u_fire > 0) {
            u += u_fire * u_fire_sign;
            u_fire -= fire_fade;
        }

        if (v_fire > 0) {
            v += v_fire * v_fire_sign;
            v_fire -= fire_fade;
        }

        if (z_odd > 0) {
            // if (u_fire <= 0) {
            //     u_fire_sign = (u > 0 && y_odd < 64) ? -1 : +1;
            // }
            u_fire = z_odd;
        }

        if (z_even > 0) {
            // if (v_fire <= 0) {
            //     v_fire_sign = (v > 0 && y_even < 64) ? -1 : +1;
            // }
            v_fire = z_even;
        }

        if (self->luma_noise > 0) {
            y_even += r_even % self->luma_noise;
            y_odd += r_odd % self->luma_noise;
        }

        if (self->chroma_noise > 0) {
            u += (int) (u * 2.f * (self->chroma_noise / 256.f)) + (r_odd % self->chroma_noise);
            v += (int) (v * 2.f * (self->chroma_noise / 256.f)) + (r_even % self->chroma_noise);
        }

        if (self->echo_offset >= 1 && i >= self->echo_offset) {
            y_even += (y_even - even[(i - self->echo_offset) * 4]) / 2;
            y_odd += (y_odd - odd[(i - self->echo_offset) * 4]) / 2;
        }

        even[i * 4 + 0] = clamp_byte(y_even);
        even[i * 4 + 1] = clamp_byte(v + 128);

        odd[i * 4 + 0] = clamp_byte(y_odd);
        odd[i * 4 + 1] = clamp_byte(u + 128);

        r_even = juice(r_even);
        r_odd = juice(r_odd);
    }
}

/**
 * Filtering Stage 3. Two consecutive YUV pixel rows, filtered in previous stages,
 * now converted to RGB in place. But conversion isn't straightforward: to make
 * the image look more analog, a sophisticated method is used.
 */
static void convert_pair_to_rgb(struct secamiz0r *self, uint8_t *even, uint8_t *odd)
{
    int const luma_loss = 4;
    int const chroma_loss = 8;
    
    for (size_t i = 0; i < self->width; i++) {
        float y_even = 0.f;
        float y_odd = 0.f;
        float u = 0.f;
        float v = 0.f;

        for (int j = 0; j < luma_loss; j++) {
            size_t idx = (size_t) clamp_int((int) (i + j), 0, (int) self->width - 1);
            y_even += (float) even[4 * idx + 0];
            y_odd += (float) odd[4 * idx + 0];
        }

        for (int j = 0; j < chroma_loss; j++) {
            size_t idx = (size_t) clamp_int((int) (i + j), 0, (int) self->width - 1);
            u += (float) odd[4 * idx + 1];
            v += (float) even[4 * idx + 1];
        }

        y_even /= 255.f * luma_loss;
        y_odd /= 255.f * luma_loss;
        u /= 255.f * chroma_loss;
        v /= 255.f * chroma_loss;

        rgb_from_yuv(&even[i * 4 + 0], y_even, u, v);
        rgb_from_yuv(&odd[i * 4 + 0], y_odd, u, v);
    }
}

/**
 * The whole process of filtering is done here.
 * This function is called every frame.
 */
void f0r_update(f0r_instance_t instance, double time, uint32_t const* src, uint32_t *dst)
{
    struct secamiz0r *self = instance;

    for (size_t i = 0; i < self->height; i += 2) {
        size_t even = (i + 0) * self->width;
        size_t odd = (i + 1) * self->width;

        uint8_t const *src_even = (uint8_t const *) &src[even];
        uint8_t const *src_odd = (uint8_t const *) &src[odd];

        uint8_t *dst_even = (uint8_t *) &dst[even];
        uint8_t *dst_odd = (uint8_t *) &dst[odd];

        copy_pair_as_yuv(self, dst_even, dst_odd, src_even, src_odd);
        prefilter_pair(self, dst_even, dst_odd);
        filter_pair(self, dst_even, dst_odd);
        convert_pair_to_rgb(self, dst_even, dst_odd);
    }

    self->frame_count++;
}
