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
	s32 n0 = 0, n1 = n, n2 = n+n, n3 = n+n+n;
	s32 n4 = n3+n, n5 = n3+n+n, n6 = n3+n+n+n, n7 = n3+n+n+n+n;

	//We must fill 8 rows
	u32 i0 = 0;
	u32 i1 = 4;
	u32 i2 = 8;
	u32 i3 = 12;
	u32 i4 = (tile_cnt * 8) + 0;
	u32 i5 = (tile_cnt * 8) + 4;
	u32 i6 = (tile_cnt * 8) + 8;
	u32 i7 = (tile_cnt * 8) + 12;

	//u32 y_ofs = 2;
	u32 last = 0;
	align <<= 1;
	for (u32 col = 0; col < n;) {
		for (u32 s = 0; (s < 4) && (col < n); ++s,  ++col) {

			//Calculate the offsets
			u8 *it0 = (u8*) &data[s+i0];
			u8 *it1 = (u8*) &data[s+i1];
			u8 *it2 = (u8*) &data[s+i2];
			u8 *it3 = (u8*) &data[s+i3];
			u8 *it4 = (u8*) &data[s+i4];
			u8 *it5 = (u8*) &data[s+i5];
			u8 *it6 = (u8*) &data[s+i6];
			u8 *it7 = (u8*) &data[s+i7];
			last = s + i7;

			it0[0] = ((n0+col+align) >> 3) - (col) + 128;
			it1[0] = ((n1+col+align) >> 3) - (col) + 128;
			it2[0] = ((n2+col+align) >> 3) - (col) + 128;
			it3[0] = ((n3+col+align) >> 3) - (col) + 128;
			it4[0] = ((n4+col+align) >> 3) - (col) + 128;
			it5[0] = ((n5+col+align) >> 3) - (col) + 128;
			it6[0] = ((n6+col+align) >> 3) - (col) + 128;
			it7[0] = ((n7+col+align) >> 3) - (col) + 128;

			it0[1] = ((n0+col+align) & 0x7) - (0) + 128;
			it1[1] = ((n1+col+align) & 0x7) - (1) + 128;
			it2[1] = ((n2+col+align) & 0x7) - (2) + 128;
			it3[1] = ((n3+col+align) & 0x7) - (3) + 128;
			it4[1] = ((n4+col+align) & 0x7) - (4) + 128;
			it5[1] = ((n5+col+align) & 0x7) - (5) + 128;
			it6[1] = ((n6+col+align) & 0x7) - (6) + 128;
			it7[1] = ((n7+col+align) & 0x7) - (7) + 128;
		}
		i0 += 16;
		i1 += 16;
		i2 += 16;
		i3 += 16;
		i4 += 16;
		i5 += 16;
		i6 += 16;
		i7 += 16;
	}

	// Correct the last 2, 4 or 6 entries...
	for (u32 i = 0; i < align; ++i) {
		u8 *it = (u8*) &indtex_data[last];
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
	u32 tile_cnt = ((n-1)>>2)+1;
	u32 i0 = 0;
	u32 i1 = 4;
	u32 i2 = 8;
	u32 i3 = 12;

	s32 n0 = 0, n1 = n, n2 = n+n, n3 = n+n+n;
	u32 last = 0;
	for (u32 col = 0; col < n;) {
		for (u32 s = 0; (s < 4) && (col < n) ; ++s,  ++col) {
			//Calculate the offsets
			u8 *it0 = (u8*) &data[s+i0];
			u8 *it1 = (u8*) &data[s+i1];
			u8 *it2 = (u8*) &data[s+i2];
			u8 *it3 = (u8*) &data[s+i3];

			it0[0] = ((n0+col+align) >> 2) - (col) + 128;
			it1[0] = ((n1+col+align) >> 2) - (col) + 128;
			it2[0] = ((n2+col+align) >> 2) - (col) + 128;
			it3[0] = ((n3+col+align) >> 2) - (col) + 128;

			it0[1] = ((n0+col+align) & 0x3) - (0) + 128;
			it1[1] = ((n1+col+align) & 0x3) - (1) + 128;
			it2[1] = ((n2+col+align) & 0x3) - (2) + 128;
			it3[1] = ((n3+col+align) & 0x3) - (3) + 128;

			last = s+i3;
		}
		i0 += 16;
		i1 += 16;
		i2 += 16;
		i3 += 16;
	}

	// Correct the last 1, 2 or 3 entries...
	for (u32 i = 0; i < align; ++i) {
		u8 *it = (u8*) &indtex_data[last];
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
	u32 tile_cnt = (((n-1)>>2)+1) << 1;
	n *= 2;	// Almost the same as if we made a 8bpp indirect of double the width
	// We only must fill 4 rows
	u32 i0 = 0;
	u32 i1 = 4;
	u32 i2 = 8;
	u32 i3 = 12;

	s32 n0 = 0, n1 = n, n2 = n+n, n3 = n+n+n;
	u32 last = 0;
	for (u32 col = 0; col < n;) {
		for (u32 s = 0; (s < 4) && (col < n) ; ++s,  ++col) {
			//Calculate the offsets
			//Only difference with 8bpp is the number of bits used from n+col
			u8 *it0 = (u8*) &data[s+i0];
			u8 *it1 = (u8*) &data[s+i1];
			u8 *it2 = (u8*) &data[s+i2];
			u8 *it3 = (u8*) &data[s+i3];

			it0[0] = ((n0+col+align) >> 2) + 128 - (col);
			it1[0] = ((n1+col+align) >> 2) + 128 - (col);
			it2[0] = ((n2+col+align) >> 2) + 128 - (col);
			it3[0] = ((n3+col+align) >> 2) + 128 - (col);

			it0[1] = ((n0+col+align) & 0x3) + 128 - (0);
			it1[1] = ((n1+col+align) & 0x3) + 128 - (1);
			it2[1] = ((n2+col+align) & 0x3) + 128 - (2);
			it3[1] = ((n3+col+align) & 0x3) + 128 - (3);

			last = s+i3;
		}
		i0 += 16;
		i1 += 16;
		i2 += 16;
		i3 += 16;
	}

	// Correct the last 1, 2 or 3 entries...
	for (u32 i = 0; i < align; ++i) {
		u8 *it = (u8*) &indtex_data[last];
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

//Uses the sprite size (from 0 to 62) to make use of indirect textrue conversion
void SGX_SpriteConverterSet(u32 width, u32 bpp_id, u32 align)
{
	//If width is greater than 8 pixels or is using 16bpp
	u32 use_indirect = (width + (bpp_id & SPRITE_16BPP)) > 1;
	GX_SetNumIndStages(use_indirect);
	if (use_indirect) {
		//Check if indirect texture is preloaded (if not then generate and load)
		u32 t = (bpp_id << 8) + (align << 6) + width;
		SGXTexPre *tex = indtex_arr + t;
		if (!tex->addr) {
			u32 tile_cnt;
			tex->addr = preloadtex_addr | 0x200000 | 0xD8000;
			tex->attr = TEX_ATTR(GX_CLAMP, GX_REPEAT);
			switch (bpp_id) {
				case SPRITE_4BPP:
					tile_cnt = __indtex4bppGen((u16*)(indtex_data + ind_addr), width, align);
					tex->fmt = TEX_FMT(GX_TF_IA8, width, 8);
					break;
				case SPRITE_8BPP:
					tile_cnt = __indtex8bppGen((u16*)(indtex_data + ind_addr), width, align);
					tex->fmt = TEX_FMT(GX_TF_IA8, width, 4);
					break;
				default:
					tile_cnt = __indtex16bppGen((u16*)(indtex_data + ind_addr), width, align);
					tex->fmt = TEX_FMT(GX_TF_IA8, (width << 1), 4);
					break;
			}
			DCStoreRange(indtex_data + ind_addr, tile_cnt * 32);
			//TODO: Directly load usign BP Registers
			SGX_PreloadTex(indtex_data + ind_addr, tex->addr, TEXPRE_TYPE_16BPP | tile_cnt);
			ind_addr += tile_cnt * 32;
			ind_addr = (ind_addr > 0x7C00 ? 0 : ind_addr);
			preloadtex_addr += tile_cnt;
		}
		SGX_SetTexPreloaded(GX_TEXMAP7, tex);
		GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_0 + (bpp_id >> 1), GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP7);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_8 - (bpp_id >> 1), GX_ITS_1);
	} else {
		GX_SetTevDirect(GX_TEVSTAGE0);
	}
}
