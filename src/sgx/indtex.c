#include "sgx.h"


#define INDTEX_SIZE			0x8000

static u8 indtex_data[INDTEX_SIZE] ATTRIBUTE_ALIGN(32);
static u32 ind_addr = 0;
static SGXTexPre indtex_arr[3*4*64];
u32 preloadtex_addr = 0x60000 >> 5;	//Begin address of preloaded textures

// 4bpp indirect
// n is the width divided by 8
static u32 __indtex4bppGen(u16 *data, u32 n, u32 align)
{
	u32 tile_cnt = (((n-1)>>2)+1) << 1;
	align <<= 1;
	u32 n0 = align;

	//We must fill 8 rows
	u16 *i0 = data + (0);
	u16 *i1 = data + (4);
	u16 *i2 = data + (8);
	u16 *i3 = data + (12);
	u16 *i4 = data + (0  + (tile_cnt << 3));
	u16 *i5 = data + (4  + (tile_cnt << 3));
	u16 *i6 = data + (8  + (tile_cnt << 3));
	u16 *i7 = data + (12 + (tile_cnt << 3));

	u32 last_v = 12 + (tile_cnt << 3);
	//u32 y_ofs = 2;
	u32 last = 12 + (tile_cnt << 3);
	u32 xc = 128;
	for (u32 col = 0; col < n;) {
		for (u32 s = 0; (s < 4) && (col < n); ++s,  ++col) {
			u32 yc = 128;
			u32 nn = n0;
			//Calculate the offsets
			*i0++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i1++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i2++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i3++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i4++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i5++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i6++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;
			*i7++ = ((xc + (nn >> 3)) << 8) | ((yc--) + (nn & 0x7)); nn += n;

			++n0;
			--xc;
			last = s+last_v;
		}
		i0 += 12; i1 += 12; i2 += 12; i3 += 12;
		i4 += 12; i5 += 12; i6 += 12; i7 += 12;
		last_v += 16;
	}

	// Correct the last 2, 4 or 6 entries...
	for (u32 i = 0; i < align; ++i) {
		u8 *it = (u8*) &data[last];
		it[0] -= (n);
		it[1] += (8);

		if (last & 0x3) {
			--last;
		} else {
			if (n <= 4) {
				last = last + (n - 5);
			} else {
				last -= 13;
			}
		}
	}

	return tile_cnt;	//Return the number of 32-byte tiles stored
}


// 8bpp indirect
// n is the width divided by 8
static u32 __indtex8bppGen(u16 *data, u32 n, u32 align)
{
	// We only must fill 4 rows
	u32 tile_cnt = (((n-1)>>2)+1);
	u32 n0 = align;

	//We must fill 8 rows
	u16 *i0 = data + (0);
	u16 *i1 = data + (4);
	u16 *i2 = data + (8);
	u16 *i3 = data + (12);

	u32 last_v = 12;
	u32 last = 0;
	u32 xc = 128;
	for (u32 col = 0; col < n;) {
		for (u32 s = 0; (s < 4) && (col < n); ++s,  ++col) {
			u32 yc = 128;
			u32 nn = n0;
			//Calculate the offsets
			*i0++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;
			*i1++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;
			*i2++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;
			*i3++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;

			++n0;
			--xc;
			last = s+last_v;
		}
		i0 += 12; i1 += 12; i2 += 12; i3 += 12;
		last_v += 16;
	}

	// Correct the last 1, 2 or 3 entries...
	for (u32 i = 0; i < align; ++i) {
		u8 *it = (u8*) &data[last];
		it[0] -= (n);
		it[1] += (4);

		if (last & 0x3) {
			--last;
		} else {
			if (n <= 2) {
				last = last + (n - 3);
			} else {
				last -= 13;
			}
		}
	}

	return tile_cnt;	//Return the number of bytes stored
}



// 16bpp indirect
// n is the width divided by 8
static u32 __indtex16bppGen(u16 *data, u32 n, u32 align)
{
	n *= 2;	// Almost the same as if we made a 8bpp indirect of double the width
	u32 tile_cnt = (((n-1)>>2)+1);
	u32 n0 = align;

	//We must fill 8 rows
	u16 *i0 = data + (0);
	u16 *i1 = data + (4);
	u16 *i2 = data + (8);
	u16 *i3 = data + (12);

	u32 last_v = 12;
	u32 last = 0;
	u32 xc = 128;
	for (u32 col = 0; col < n;) {
		for (u32 s = 0; (s < 4) && (col < n); ++s,  ++col) {
			u32 yc = 128;
			u32 nn = n0;
			//Calculate the offsets
			*i0++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;
			*i1++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;
			*i2++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;
			*i3++ = ((xc + (nn >> 2)) << 8) | ((yc--) + (nn & 0x3)); nn += n;

			++n0;
			--xc;
			last = s+last_v;
		}
		i0 += 12; i1 += 12; i2 += 12; i3 += 12;
		last_v += 16;
	}


	// Correct the last 1, 2 or 3 entries...
	for (u32 i = 0; i < align; ++i) {
		u8 *it = (u8*) &data[last];
		it[0] -= (n); //XXX: could be wrong
		it[1] += (4);


		if (last & 0x3) {
			--last;
		} else {
			if (n <= 2) {
				last = last + (n - 3);
			} else {
				last -= 13;
			}
		}
	}

	return tile_cnt;	//Return the number of bytes stored
}


void SGX_InitSpriteConv(void)
{
	ind_addr = 0;

}

void __genConverter(SGXTexPre *tex, u32 width, u32 bpp_id, u32 align)
{
	u32 tile_cnt;
	tex->addr = preloadtex_addr | (0x200000 | 0xD8000);
	tex->attr = TEX_ATTR(GX_CLAMP, GX_REPEAT);
	//Check bpp to generate texture
	if (bpp_id == SPRITE_4BPP) {
		tile_cnt = __indtex4bppGen((u16*)(indtex_data + ind_addr), width, align);
		tex->fmt = TEX_FMT(GX_TF_IA8, width, 8);
	} else {
		// 8bpp is the same as 16bpp if the width is doubled
		width <<= bpp_id == SPRITE_16BPP;
		tile_cnt = __indtex8bppGen((u16*)(indtex_data + ind_addr), width, align);
		tex->fmt = TEX_FMT(GX_TF_IA8, width, 4);
	}
	DCStoreRange(indtex_data + ind_addr, tile_cnt * 32);
	//TODO: Directly load usign BP Registers
	SGX_PreloadTex(indtex_data + ind_addr, tex->addr, TEXPRE_TYPE_16BPP | tile_cnt);
	ind_addr += tile_cnt * 32;
	ind_addr = (ind_addr > 0x7C00 ? 0 : ind_addr);
	preloadtex_addr += tile_cnt;
}

//Uses the sprite size (from 0 to 62) to make use of indirect textrue conversion
void SGX_SpriteConverterSet(u32 width, u32 bpp_id, u32 align)
{
	//If width is greater than 8 pixels or is using 16bpp
	u32 use_indirect = (width + align + (bpp_id & SPRITE_16BPP)) > 1;
	GX_SetNumIndStages(use_indirect);
	if (use_indirect) {
		//Check if indirect texture is preloaded (if not then generate and load)
		u32 t = (bpp_id << 8) + (align << 6) + width;
		SGXTexPre *tex = indtex_arr + t;
		if (!tex->addr) {
			__genConverter(tex, width, bpp_id, align);
		}
		SGX_SetTexPreloaded(GX_TEXMAP7, tex);
		GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_0 + (bpp_id >> 1), GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP7);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_8 - (bpp_id >> 1), GX_ITS_1);
	} else {
		GX_SetTevDirect(GX_TEVSTAGE0);
	}
}


static const u16 indtex_8x2cell[] ATTRIBUTE_ALIGN(32) = {
	0x8080, 0x7F81, 0x0000, 0x0000,
	0x817F, 0x8080, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 indtex_16x1cell[] ATTRIBUTE_ALIGN(32) = {
	0x8080, 0x7f81, 0x0000, 0x0000,
	0x8081, 0x7f82, 0x0000, 0x0000,
	0x817e, 0x807f, 0x0000, 0x0000,
	0x817f, 0x8080, 0x0000, 0x0000
};

static const u16 indtex_16x2cell[] ATTRIBUTE_ALIGN(32) = {
	0x8080, 0x7f81, 0x7e84, 0x7d85,
	0x8081, 0x7f82, 0x7e85, 0x7d86,
	0x817e, 0x807f, 0x7f82, 0x7e83,
	0x817f, 0x8080, 0x7f83, 0x7e84,

	0x827c, 0x817d, 0x8080, 0x7f81,
	0x827d, 0x817e, 0x8081, 0x7f82,
	0x837a, 0x827b, 0x817e, 0x807f,
	0x837b, 0x827c, 0x817f, 0x8080
};

enum IndCell {
	//CELL FORMAT
	INDTEX_CELL_8x2,
	INDTEX_CELL_16x1,
	INDTEX_CELL_16x2,
	INDTEX_CELL_32x1,
	INDTEX_CELL_32x2,

	//BITMAP FORMAT
	INDTEX_BITMAP_4x512,	// 4bpp
	INDTEX_BITMAP_4x1024,
	INDTEX_BITMAP_8x512,	// 8bpp
	INDTEX_BITMAP_8x1024,
	INDTEX_BITMAP_16x512,	// 16bpp
	INDTEX_BITMAP_16x1024,
	INDTEX_BITMAP_32x512,	// 32bpp
	INDTEX_BITMAP_32x1024
};

SGXTexPre indtex_cell[16] = {
	{.addr = (0x7FF00 >> 5) | (0x200000 | 0xD8000), .fmt = TEX_FMT(GX_TF_IA8, 2, 2), .attr = TEX_ATTR(GX_CLAMP, GX_REPEAT)},
	{.addr = (0x7FF20 >> 5) | (0x200000 | 0xD8000), .fmt = TEX_FMT(GX_TF_IA8, 2, 4), .attr = TEX_ATTR(GX_CLAMP, GX_REPEAT)},
	{.addr = (0x7FF40 >> 5) | (0x200000 | 0xD8000), .fmt = TEX_FMT(GX_TF_IA8, 4, 8), .attr = TEX_ATTR(GX_CLAMP, GX_REPEAT)},
};

void SGX_CellConverterInit(void)
{
	SGX_PreloadTex(indtex_8x2cell , indtex_cell[INDTEX_CELL_8x2].addr, TEXPRE_TYPE_16BPP | 1);
	SGX_PreloadTex(indtex_16x1cell, indtex_cell[INDTEX_CELL_16x1].addr, TEXPRE_TYPE_16BPP | 1);
	SGX_PreloadTex(indtex_16x2cell, indtex_cell[INDTEX_CELL_16x2].addr, TEXPRE_TYPE_16BPP | 2);

	//Generate the Bitmap Converters
	__genConverter(&indtex_cell[INDTEX_BITMAP_4x512], (512-8)/8, SPRITE_4BPP, 0);
	__genConverter(&indtex_cell[INDTEX_BITMAP_8x512], (512-8)/8, SPRITE_8BPP, 0);
	__genConverter(&indtex_cell[INDTEX_BITMAP_16x512], (512-8)/8, SPRITE_16BPP, 0);

	//Same indirect texture
	//__genConverter(&indtex_cell[INDTEX_BITMAP_32x512], width, bpp_id, align);
}

void SGX_CellConverterSet(u32 cellsize, u32 bpp_id)
{
	if (cellsize && (bpp_id == SPRITE_8BPP)) {
		GX_SetNumIndStages(1);
		SGX_SetTexPreloaded(GX_TEXMAP7, &indtex_cell[INDTEX_CELL_8x2]);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP7);
		GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_2, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_8, GX_ITS_4);
	} else if (bpp_id == SPRITE_16BPP) {
		GX_SetNumIndStages(1);
		SGX_SetTexPreloaded(GX_TEXMAP7, &indtex_cell[INDTEX_CELL_16x1 + cellsize]);
		GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_1, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP7);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_4, GX_ITS_1);
	}
	//TODO: Handle Cell 32BPP (1x1 and 2x2)
}

u32 SGX_BitmapConverterSet(u32 width, u32 bpp_id)
{
	SGXTexPre *indtex = &indtex_cell[INDTEX_BITMAP_4x512 + (bpp_id << 1) + width];
	//TODO: We should be able to handle all cases here...
	if (!indtex->addr) {
		return 0;
	}
	GX_SetNumIndStages(1);
	SGX_SetTexPreloaded(GX_TEXMAP7, indtex);
	GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_0 + (bpp_id >> 1), GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
	GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP7);
	GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_8 - (bpp_id >> 1), GX_ITS_1);
	return 1;
}
