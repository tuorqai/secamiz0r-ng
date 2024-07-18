
#include <math.h>
#include <stdlib.h>
#include "frei0r.h"

// #define ENABLE_TIME_TEST

struct secamiz0r
{
    unsigned int width;
    unsigned int height;
    size_t frame_count;
    double intensity;
};

int f0r_init()
{
	return 1;
}

void f0r_deinit()
{
}

void f0r_get_plugin_info(f0r_plugin_info_t *info)
{
    info->name = "secamiz0r";
    info->author = "tuorqai";
    info->plugin_type = F0R_PLUGIN_TYPE_FILTER;
    info->color_model = F0R_COLOR_MODEL_RGBA8888;
    info->frei0r_version = FREI0R_MAJOR_VERSION;
    info->major_version = 1;
    info->minor_version = 0;
    info->num_params = 1;
    info->explanation = "SECAM Fire effect";
}

void f0r_get_param_info(f0r_param_info_t *info, int index)
{
    switch (index) {
    case 0:
        info->name = "Intensity";
        info->explanation = NULL;
        info->type = F0R_PARAM_DOUBLE;
        break;
    default:
        break;
    }
}

f0r_instance_t f0r_construct(unsigned int width, unsigned int height)
{
    struct secamiz0r *self = malloc(sizeof(*self));

    if (!self) {
        return NULL;
    }

    self->width = width;
    self->height = height;
    self->frame_count = 0;
    self->intensity = 0.125f;

    return self;
}

void f0r_destruct(f0r_instance_t instance)
{
    free(instance);
}

void f0r_set_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *self = instance;

    switch (index) {
    case 0:
        self->intensity = *((double const *) param);
        break;
    default:
        break;
    }
}

void f0r_get_param_value(f0r_instance_t instance, f0r_param_t param, int index)
{
    struct secamiz0r *self = instance;

    switch (index) {
    case 0:
        *((double *) param) = self->intensity;
        break;
    default:
        break;
    }
}

static void unpack_rgb(float *rgb, uint8_t const *src)
{
    rgb[0] = ((float) src[0]) / 255.f;
    rgb[1] = ((float) src[1]) / 255.f;
    rgb[2] = ((float) src[2]) / 255.f;
}

static uint8_t y_from_rgb(float const *rgb)
{
    return (uint8_t) (16.0 + (65.7380 * rgb[0]) + (129.057 * rgb[1]) + (25.0640 * rgb[2]));
}

static uint8_t u_from_rgb(float const *rgb)
{
    return (uint8_t) (128.0 - (37.9450 * rgb[0]) - (74.4940 * rgb[1]) + (112.439 * rgb[2]));
}

static uint8_t v_from_rgb(float const *rgb)
{
    return (uint8_t) (128.0 + (112.439 * rgb[0]) - (94.1540 * rgb[1]) - (18.2850 * rgb[2]));
}

static int clamp_int(int value, int a, int b)
{
    return (value < a) ? a : ((value > b) ? b : value);
}

static uint8_t clamp_byte(int value)
{
    return (uint8_t) clamp_int(value, 0, 255);
}

static void rgb_from_yuv(uint8_t *dst, float y, float u, float v)
{
    dst[0] = clamp_byte((int) ((298.082 * y) + (408.583 * v) - 222.921));
    dst[1] = clamp_byte((int) ((298.082 * y) - (100.291 * u) - (208.120 * v) + 135.576));
    dst[2] = clamp_byte((int) ((298.082 * y) + (516.412 * u) - 276.836));
}

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

static int juice(int j)
{
    j ^= j << 13;
    j ^= j >> 17;
    j ^= j << 5;

    return j;
}

static unsigned int umod(int a, int b)
{
    return ((a % b) + b) % b;
}

static void prefilter_pair(struct secamiz0r *self, uint8_t *even, uint8_t *odd)
{
    int const oscillation_threshold = 1024 - (int) (self->intensity * 256.0);

    int r_even = rand();
    int r_odd = rand();

    int y_even_prev = 0;// even[0];
    int y_odd_prev = 0;// odd[0];

    int y_even_oscillation = 0;
    int y_odd_oscillation = 0;

    for (int i = self->width - 1; i >= 0; i--) {
        int y_even = even[i * 4 + 0];
        int y_odd = odd[i * 4 + 0];

        int u_fire = 0;
        int v_fire = 0;

        y_even_oscillation += abs(y_even - y_even_prev - umod(r_even, 512));
        y_odd_oscillation += abs(y_odd - y_odd_prev - umod(r_odd, 512));

        if (y_even_oscillation > oscillation_threshold) {
            even[i * 4 + 2] = umod(r_even, 80);
        }

        if (y_odd_oscillation > oscillation_threshold) {
            odd[i * 4 + 2] = umod(r_odd, 80);
        }

        r_even = juice(r_even);
        r_odd = juice(r_odd);

        y_even_prev = y_even;
        y_odd_prev = y_odd;

        y_even_oscillation /= 2;
        y_odd_oscillation /= 2;
    }
}

static void filter_pair(struct secamiz0r *self, uint8_t *even, uint8_t *odd)
{
    prefilter_pair(self, even, odd);

    int const noise = (int) (self->intensity * self->intensity * 256.f);
    int const echo = (int) (self->intensity * 8.f);

    int r_even = rand();
    int r_odd = rand();

    int u_fire = 0;
    int u_fire_sign = 0;

    int v_fire = 0;
    int v_fire_sign = 0;

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
            if (u_fire <= 0) {
                u_fire_sign = (u > 0 && y_odd < 64) ? -1 : +1;
            }
            u_fire = z_odd;
        }

        if (z_even > 0) {
            if (v_fire <= 0) {
                v_fire_sign = (v > 0 && y_even < 64) ? -1 : +1;
            }
            v_fire = z_even;
        }

        if (noise > 0) {
            y_even += r_even % noise;
            y_odd += r_odd % noise;

            u += (int) (u * 2.f * (noise / 256.f)) + (r_odd % noise);
            v += (int) (v * 2.f * (noise / 256.f)) + (r_even % noise);
        }

        if (echo >= 1 && i >= echo) {
            y_even += (y_even - even[(i - echo) * 4]) / 2;
            y_odd += (y_odd - odd[(i - echo) * 4]) / 2;
        }

        even[i * 4 + 0] = clamp_byte(y_even);
        even[i * 4 + 1] = clamp_byte(v + 128);

        odd[i * 4 + 0] = clamp_byte(y_odd);
        odd[i * 4 + 1] = clamp_byte(u + 128);

        r_even = juice(r_even);
        r_odd = juice(r_odd);
    }
}

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

void f0r_update(f0r_instance_t instance, double time, uint32_t const* src, uint32_t *dst)
{
    struct secamiz0r *self = instance;

#ifdef ENABLE_TIME_TEST
    double d = fmod(time, 10000.0) / 10000.0;
    self->intensity = d;
#endif

    for (size_t i = 0; i < self->height; i += 2) {
        size_t even = (i + 0) * self->width;
        size_t odd = (i + 1) * self->width;

        uint8_t const *src_even = (uint8_t const *) &src[even];
        uint8_t const *src_odd = (uint8_t const *) &src[odd];

        uint8_t *dst_even = (uint8_t *) &dst[even];
        uint8_t *dst_odd = (uint8_t *) &dst[odd];

        copy_pair_as_yuv(self, dst_even, dst_odd, src_even, src_odd);
        filter_pair(self, dst_even, dst_odd);
        convert_pair_to_rgb(self, dst_even, dst_odd);
    }

#ifdef ENABLE_TIME_TEST
    int w = (int) floor(d * self->width);
    for (int x = 0; x < w; x++) {
        dst[(self->height - 8) * self->width + x] = 0xffffffff;
        for (size_t y = (self->height - 7); y < self->height; y++) {
            dst[y * self->width + x] = 0xffff0000;
        }
    }
#endif

    self->frame_count++;
}
