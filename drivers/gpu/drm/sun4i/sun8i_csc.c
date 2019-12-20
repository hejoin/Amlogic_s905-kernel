// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Jernej Skrabec <jernej.skrabec@siol.net>
 */

#include <drm/drm_print.h>

#include "sun8i_csc.h"
#include "sun8i_mixer.h"

static const u32 ccsc_base[2][2] = {
	{CCSC00_OFFSET, CCSC01_OFFSET},
	{CCSC10_OFFSET, CCSC11_OFFSET},
};

/*
 * Factors are in two's complement format, 10 bits for fractinal part.
 * First tree values in each line are multiplication factor and last
 * value is constant, which is added at the end.
 */

static const u32 yuv2rgb[2][2][12] = {
	[DRM_COLOR_YCBCR_LIMITED_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x000004A8, 0x00000000, 0x00000662, 0xFFFC8451,
			0x000004A8, 0xFFFFFE6F, 0xFFFFFCC0, 0x00021E4D,
			0x000004A8, 0x00000811, 0x00000000, 0xFFFBACA9,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x000004A8, 0x00000000, 0x0000072B, 0xFFFC1F99,
			0x000004A8, 0xFFFFFF26, 0xFFFFFDDF, 0x00013383,
			0x000004A8, 0x00000873, 0x00000000, 0xFFFB7BEF,
		}
	},
	[DRM_COLOR_YCBCR_FULL_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x00000400, 0x00000000, 0x0000059B, 0xFFFD322E,
			0x00000400, 0xFFFFFEA0, 0xFFFFFD25, 0x00021DD5,
			0x00000400, 0x00000716, 0x00000000, 0xFFFC74BD,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x00000400, 0x00000000, 0x0000064C, 0xFFFCD9B4,
			0x00000400, 0xFFFFFF41, 0xFFFFFE21, 0x00014F96,
			0x00000400, 0x0000076C, 0x00000000, 0xFFFC49EF,
		}
	},
};

static const u32 yvu2rgb[2][2][12] = {
	[DRM_COLOR_YCBCR_LIMITED_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x000004A8, 0x00000662, 0x00000000, 0xFFFC8451,
			0x000004A8, 0xFFFFFCC0, 0xFFFFFE6F, 0x00021E4D,
			0x000004A8, 0x00000000, 0x00000811, 0xFFFBACA9,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x000004A8, 0x0000072B, 0x00000000, 0xFFFC1F99,
			0x000004A8, 0xFFFFFDDF, 0xFFFFFF26, 0x00013383,
			0x000004A8, 0x00000000, 0x00000873, 0xFFFB7BEF,
		}
	},
	[DRM_COLOR_YCBCR_FULL_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x00000400, 0x0000059B, 0x00000000, 0xFFFD322E,
			0x00000400, 0xFFFFFD25, 0xFFFFFEA0, 0x00021DD5,
			0x00000400, 0x00000000, 0x00000716, 0xFFFC74BD,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x00000400, 0x0000064C, 0x00000000, 0xFFFCD9B4,
			0x00000400, 0xFFFFFE21, 0xFFFFFF41, 0x00014F96,
			0x00000400, 0x00000000, 0x0000076C, 0xFFFC49EF,
		}
	},
};

/*
 * DE3 has a bit different CSC units. Factors are in two's complement format.
 * First three factors in a row are multiplication factors which have 17 bits
 * for fractional part. Fourth value in a row is comprised of two factors.
 * Upper 16 bits represents difference, which is subtracted from the input
 * value before multiplication and lower 16 bits represents constant, which
 * is addes at the end.
 *
 * x' = c00 * (x + d0) + c01 * (y + d1) + c02 * (z + d2) + const0
 * y' = c10 * (x + d0) + c11 * (y + d1) + c12 * (z + d2) + const1
 * z' = c20 * (x + d0) + c21 * (y + d1) + c22 * (z + d2) + const2
 *
 * Please note that above formula is true only for Blender CSC. Other DE3 CSC
 * units takes only positive value for difference. From what can be deducted
 * from BSP driver code, those units probably automatically assume that
 * difference has to be subtracted.
 *
 * Layout of factors in table:
 * c00 c01 c02 [d0 const0]
 * c10 c11 c12 [d1 const1]
 * c20 c21 c22 [d2 const2]
 */

static const u32 yuv2rgb_de3[2][2][12] = {
	[DRM_COLOR_YCBCR_LIMITED_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x0002542A, 0x00000000, 0x0003312A, 0xFFC00000,
			0x0002542A, 0xFFFF376B, 0xFFFE5FC3, 0xFE000000,
			0x0002542A, 0x000408D2, 0x00000000, 0xFE000000,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x0002542A, 0x00000000, 0x000395E2, 0xFFC00000,
			0x0002542A, 0xFFFF92D2, 0xFFFEEF27, 0xFE000000,
			0x0002542A, 0x0004398C, 0x00000000, 0xFE000000,
		}
	},
	[DRM_COLOR_YCBCR_FULL_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x00020000, 0x00000000, 0x0002CDD2, 0x00000000,
			0x00020000, 0xFFFF4FCE, 0xFFFE925D, 0xFE000000,
			0x00020000, 0x00038B43, 0x00000000, 0xFE000000,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x00020000, 0x00000000, 0x0003264C, 0x00000000,
			0x00020000, 0xFFFFA018, 0xFFFF1053, 0xFE000000,
			0x00020000, 0x0003B611, 0x00000000, 0xFE000000,
		}
	},
};

static const u32 yvu2rgb_de3[2][2][12] = {
	[DRM_COLOR_YCBCR_LIMITED_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x0002542A, 0x0003312A, 0x00000000, 0xFFC00000,
			0x0002542A, 0xFFFE5FC3, 0xFFFF376B, 0xFE000000,
			0x0002542A, 0x00000000, 0x000408D2, 0xFE000000,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x0002542A, 0x000395E2, 0x00000000, 0xFFC00000,
			0x0002542A, 0xFFFEEF27, 0xFFFF92D2, 0xFE000000,
			0x0002542A, 0x00000000, 0x0004398C, 0xFE000000,
		}
	},
	[DRM_COLOR_YCBCR_FULL_RANGE] = {
		[DRM_COLOR_YCBCR_BT601] = {
			0x00020000, 0x0002CDD2, 0x00000000, 0x00000000,
			0x00020000, 0xFFFE925D, 0xFFFF4FCE, 0xFE000000,
			0x00020000, 0x00000000, 0x00038B43, 0xFE000000,
		},
		[DRM_COLOR_YCBCR_BT709] = {
			0x00020000, 0x0003264C, 0x00000000, 0x00000000,
			0x00020000, 0xFFFF1053, 0xFFFFA018, 0xFE000000,
			0x00020000, 0x00000000, 0x0003B611, 0xFE000000,
		}
	},
};

static void sun8i_csc_set_coefficients(struct regmap *map, u32 base,
				       enum sun8i_csc_mode mode,
				       enum drm_color_encoding encoding,
				       enum drm_color_range range)
{
	const u32 *table;
	u32 base_reg;

	switch (mode) {
	case SUN8I_CSC_MODE_YUV2RGB:
		table = yuv2rgb[DRM_COLOR_YCBCR_FULL_RANGE][encoding];
		break;
	case SUN8I_CSC_MODE_YVU2RGB:
		table = yvu2rgb[DRM_COLOR_YCBCR_FULL_RANGE][encoding];
		break;
	default:
		DRM_WARN("Wrong CSC mode specified.\n");
		return;
	}

	base_reg = SUN8I_CSC_COEFF(base, 0);
	regmap_bulk_write(map, base_reg, table, 12);
}

static void sun8i_de3_ccsc_set_coefficients(struct regmap *map, int layer,
					    enum sun8i_csc_mode mode,
					    enum drm_color_encoding encoding,
					    enum drm_color_range range)
{
	const u32 *table;
	u32 base_reg;

	switch (mode) {
	case SUN8I_CSC_MODE_YUV2RGB:
		table = yuv2rgb_de3[DRM_COLOR_YCBCR_FULL_RANGE][encoding];
		break;
	case SUN8I_CSC_MODE_YVU2RGB:
		table = yvu2rgb_de3[DRM_COLOR_YCBCR_FULL_RANGE][encoding];
		break;
	default:
		DRM_WARN("Wrong CSC mode specified.\n");
		return;
	}

	base_reg = SUN50I_MIXER_BLEND_CSC_COEFF(DE3_BLD_BASE, layer, 0, 0);
	regmap_bulk_write(map, base_reg, table, 12);
}

static void sun8i_csc_enable(struct regmap *map, u32 base, bool enable)
{
	u32 val;

	if (enable)
		val = SUN8I_CSC_CTRL_EN;
	else
		val = 0;

	regmap_update_bits(map, SUN8I_CSC_CTRL(base), SUN8I_CSC_CTRL_EN, val);
}

static void sun8i_de3_ccsc_enable(struct regmap *map, int layer, bool enable)
{
	u32 val, mask;

	mask = SUN50I_MIXER_BLEND_CSC_CTL_EN(layer);

	if (enable)
		val = mask;
	else
		val = 0;

	regmap_update_bits(map, SUN50I_MIXER_BLEND_CSC_CTL(DE3_BLD_BASE),
			   mask, val);
}

void sun8i_csc_set_ccsc_coefficients(struct sun8i_mixer *mixer, int layer,
				     enum sun8i_csc_mode mode,
				     enum drm_color_encoding encoding,
				     enum drm_color_range range)
{
	u32 base;

	if (mixer->cfg->is_de3) {
		sun8i_de3_ccsc_set_coefficients(mixer->engine.regs, layer,
						mode, encoding, range);
		return;
	}

	base = ccsc_base[mixer->cfg->ccsc][layer];

	sun8i_csc_set_coefficients(mixer->engine.regs, base,
				   mode, encoding, range);
}

void sun8i_csc_enable_ccsc(struct sun8i_mixer *mixer, int layer, bool enable)
{
	u32 base;

	if (mixer->cfg->is_de3) {
		sun8i_de3_ccsc_enable(mixer->engine.regs, layer, enable);
		return;
	}

	base = ccsc_base[mixer->cfg->ccsc][layer];

	sun8i_csc_enable(mixer->engine.regs, base, enable);
}
