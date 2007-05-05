/*
 * Copyright © 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Soren Sandmann, Red Hat, Inc.
 */

#include <stdlib.h>

#include "pixman.h"
#include "pixman-private.h"

void
pixman_image_init_bits (pixman_image_t         *image,
			pixman_format_code_t    format,
			int                     width,
			int                     height,
			uint8_t		       *bits,
			int			rowstride)
{
    image_t *img = (image_t *)image;

    img->type = BITS;
    img->bits.format = format;
    img->bits.width = width;
    img->bits.height = height;
    img->bits.bits = bits;
    img->bits.rowstride = rowstride;
}

void
pixman_set_clip_region (pixman_image_t    *image,
			pixman_region16_t *region)
{
    
}

enum
{
    PIXMAN_BAD_VALUE,
    PIXMAN_BAD_ALLOC
};

static void
init_source_image (source_image_t *image)
{
    image->class = SOURCE_IMAGE_CLASS_UNKNOWN;
}

static void
init_gradient (gradient_t     *gradient,
	       int	       stop_count,
	       pixman_fixed_t *stop_points,
	       pixman_color_t *stop_colors,
	       int *error)
{
    int i;
    pixman_fixed_t dpos;

    if (stop_count <= 0)
    {
        *error = PIXMAN_BAD_VALUE;
        return;
    }

    init_source_image (&gradient->common);
    
    dpos = -1;
    for (i = 0; i < stop_count; ++i)
    {
        if (stop_points[i] < dpos || stop_points[i] > (1<<16))
	{
            *error = PIXMAN_BAD_VALUE;
            return;
        }
        dpos = stop_points[i];
    }

    gradient->stops = malloc (stop_count * sizeof (gradient_stop_t));
    if (!gradient->stops)
    {
        *error = PIXMAN_BAD_ALLOC;
        return;
    }

    gradient->n_stops = stop_count;

    for (i = 0; i < stop_count; ++i)
    {
        gradient->stops[i].x = stop_points[i];
        gradient->stops[i].color = stop_colors[i];
    }

    gradient->stop_range = 0xffff;
    gradient->color_table = NULL;
    gradient->color_table_size = 0;
}

static uint32_t
color_to_uint32 (const pixman_color_t *color)
{
    return
	(color->alpha >> 8 << 24) |
	(color->red >> 8 << 16) |
        (color->green & 0xff00) |
	(color->blue >> 8);
}

void
pixman_image_init_solid_fill (pixman_image_t *image,
			      pixman_color_t *color,
			      int            *error)
{
    image_t *priv = (image_t *)image;
    priv->type = SOLID;
    priv->solid.color = color_to_uint32 (color);
}

void
pixman_image_init_linear_gradient (pixman_image_t       *image,
				   pixman_point_fixed_t *p1,
				   pixman_point_fixed_t *p2,
				   int                   n_stops,
				   pixman_fixed_t       *stops,
				   pixman_color_t       *colors,
				   int                  *error)
{
    image_t *priv = (image_t *)image;
    linear_gradient_t *linear = &priv->linear;
    
    if (n_stops < 2)
    {
        *error = PIXMAN_BAD_VALUE;
        return;
    }
    
    init_gradient (&linear->common, n_stops, stops, colors, error);

    linear->p1 = *p1;
    linear->p2 = *p2;

    priv->type = LINEAR;
}


void
pixman_image_init_radial_gradient (pixman_image_t *image,
				   pixman_point_fixed_t *inner,
				   pixman_point_fixed_t *outer,
				   pixman_fixed_t inner_radius,
				   pixman_fixed_t outer_radius,
				   int             n_stops,
				   pixman_fixed_t *stops,
				   pixman_color_t *colors,
				   int            *error)
{
    image_t *priv = (image_t *)image;
    radial_gradient_t *radial = &priv->radial;

    if (n_stops < 2)
    {
        *error = PIXMAN_BAD_VALUE;
	return;
    }

    init_gradient (&radial->common, n_stops, stops, colors, error);

    priv->type = RADIAL;
    
    radial->c1.x = inner->x;
    radial->c1.y = inner->y;
    radial->c1.radius = inner_radius;
    radial->c2.x = outer->x;
    radial->c2.y = outer->y;
    radial->c2.radius = outer_radius;
    radial->cdx = (radial->c2.x - radial->c1.x) / 65536.;
    radial->cdy = (radial->c2.y - radial->c1.y) / 65536.;
    radial->dr = (radial->c2.radius - radial->c1.radius) / 65536.;
    radial->A = (  radial->cdx * radial->cdx
		   + radial->cdy * radial->cdy
		   - radial->dr  * radial->dr);
}

void
pixman_image_init_conical_gradient (pixman_image_t *image,
				    pixman_point_fixed_t *center,
				    pixman_fixed_t angle,
				    int n_stops,
				    pixman_fixed_t *stops,
				    pixman_color_t *colors,
				    int *error)
{
    image_t *priv = (image_t *)image;
    conical_gradient_t *conical = &priv->conical;

    if (n_stops < 2)
    {
	*error = PIXMAN_BAD_VALUE;
	return;
    }

    init_gradient (&conical->common, n_stops, stops, colors, error);

    priv->type = CONICAL;
    conical->center = *center;
    conical->angle = angle;
}

#define SCANLINE_BUFFER_LENGTH 1024

void
pixman_composite (pixman_image_t	*src_img,
		  pixman_image_t	*mask_img,
		  pixman_image_t	*dest_img,
		  int			 src_x,
		  int			 src_y,
		  int			 mask_x,
		  int			 mask_y,
		  int			 dest_x,
		  int			 dset_y,
		  int			 width,
		  int			 height)
{
    image_t *src = (image_t *) src;
    image_t *mask = (image_t *) mask;
    image_t *dest = (image_t *) dest;

    uint32_t _scanline_buffer[SCANLINE_BUFFER_LENGTH * 3];
    uint32_t *scanline_buffer = _scanline_buffer;

    if (width > SCANLINE_BUFFER_LENGTH)
	scanline_buffer = (uint32_t *)malloc (width * 3 * sizeof (uint32_t));

    if (!scanline_buffer)
	return;

    
    
    if (scanline_buffer != _scanline_buffer)
	free (scanline_buffer);
}