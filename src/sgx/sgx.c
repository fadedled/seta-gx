
#include "sgx.h"
#include "../vidshared.h"
#include "../vdp1.h"
#include "../vdp2.h"
#include "../osd/gui.h"

#include "../../res/tex_swizzler.inc"

#define TREG_VDPTEX			0
#define TREG_RGBTEX			0
#define TREG_LINEARCONV		1

#define NUM_SPRITE_WIDTHS	64



#define GX_LOAD_BP_REG(x)				\
do {								\
	wgPipe->U8 = 0x61;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(x);		\
	asm volatile ("" ::: "memory" ); \
} while(0)

#define GX_LOAD_CP_REG(x, y)			\
do {								\
	wgPipe->U8 = 0x08;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U8 = (u8)(x);			\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(y);		\
	asm volatile ("" ::: "memory" ); \
} while(0)

#define GX_LOAD_XF_REG(x, y)			\
do {								\
	wgPipe->U8 = 0x10;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)((x)&0xffff);		\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(y);		\
	asm volatile ("" ::: "memory" ); \
} while(0)

#define GX_LOAD_XF_REGS(x, n)			\
do {								\
	wgPipe->U8 = 0x10;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(((((n)&0xffff)-1)<<16)|((x)&0xffff));				\
	asm volatile ("" ::: "memory" ); \
} while(0)



//XXX: divide?
static u16 tlut_14bpp_ram[0x800] ATTRIBUTE_ALIGN(32);
static u16 tlut_8bpp_ram[0x800] ATTRIBUTE_ALIGN(32);
static u16 tlut_4bpp_ram[0x800] ATTRIBUTE_ALIGN(32);
static u8 tlut_dirty[0x80] ATTRIBUTE_ALIGN(32);
static GXTlutRegion tlut_region;	//XXX:tlut region for callback (can skip?)

static GXTexRegion tex_region[3];
//static GXTexRegion indtex_regions[192];
//static GXTexObj    indtex_objects[192];

//Tex Swizzler are stored by size, format and aligment
static GXTexRegion ind_regions[64][3][4];
static GXTexObj    ind_texs[64][3][4];

static GXTexRegion ind_cellreg8;
static GXTexObj    ind_celltex8;

//==============================================================================
//Structs for gx tlutregions and tlutobj to see if we can configure them directly

//XXX: Horribly hacky, skip this and use display lists
struct __gx_tlutregion
{
	u32 tmem_addr_conf;
	u32 tmem_addr_base;
	u32 tlut_maddr;
	u16 tlut_nentries;
	u8 _pad[2];
} __attribute__((packed));

struct __gx_tlutobj
{
	u32 tlut_fmt;
	u32 tlut_maddr;
	u16 tlut_nentries;
	u8 _pad[2];
} __attribute__((packed));
//==============================================================================

static const f32 tex_array[] = {
	0.0f, 0.0f,
	8.0f, 0.0f,
	0.0f, 8.0f,
	8.0f, 8.0f
};

static const u16 cell8_tex[] ATTRIBUTE_ALIGN(32) = {
	0x8080, 0x7F81, 0x0000, 0x0000,
	0x817F, 0x8080, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000
};

static GXTlutRegion* __SGX_CalcTlutRegion(u32 idx)
{
	struct __gx_tlutregion *ptr = (struct __gx_tlutregion*) &tlut_region;
	ptr->tmem_addr_conf = 0x65000000 | idx;
	ptr->tmem_addr_base = (GX_TL_RGB5A3 << 10) | (ptr->tmem_addr_conf & 0x3ff);
	return &tlut_region;
}

static GXTexRegion* __SGX_CalcTexRegion(const GXTexObj *obj, u8 mapid)
{
	if (GX_GetTexObjFmt(obj) == GX_TF_RGBA8) {
		return &tex_region[1];
	}
	return &tex_region[0];
}

void SGX_Init(void)
{
	gui_Init();
	GX_SetArray(GX_VA_TEX0, tex_array, sizeof(*tex_array) * 2);

	//Set the texcache regions
	GX_InitTexCacheRegion(&tex_region[0], GX_FALSE, 0, GX_TEXCACHE_128K, 0, GX_TEXCACHE_NONE);
	GX_InitTexCacheRegion(&tex_region[1], GX_FALSE, 0, GX_TEXCACHE_32K,  0x80000, GX_TEXCACHE_32K);
	GX_SetTexRegionCallback(__SGX_CalcTexRegion);
	GX_SetTlutRegionCallback(__SGX_CalcTlutRegion);

	//337920 bytes of indirect textures
	u32 tex_ofs_tmp = 0;
	u32 tex_ofs = 0;
	for (u32 i = 1; i < 64; ++i) {
		u32 size = ((i >> 2) + ((i & 0x3) > 0)) * 64;
		for (u32 align = 0; align < 4; ++align) {
			GX_InitTexObj(&ind_texs[i][SPRITE_4BPP][align], (void*) &tex_swizzler_bin[tex_ofs], i, 8, GX_TF_IA8, GX_CLAMP, GX_REPEAT, GX_FALSE);
			GX_InitTexObjLOD(&ind_texs[i][SPRITE_4BPP][align], GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
			GX_InitTexPreloadRegion(&ind_regions[i][SPRITE_4BPP][align], 0x20000 + tex_ofs, size, 0, GX_TEXCACHE_NONE);
			GX_PreloadEntireTexture(&ind_texs[i][SPRITE_4BPP][align], &ind_regions[i][SPRITE_4BPP][align]);
			tex_ofs += size;
		}
	}

	for (u32 i = 1; i < 64; ++i) {
		u32 size = ((i >> 2) + ((i & 0x3) > 0)) * 32;
		for (u32 align = 0; align < 4; ++align) {
			GX_InitTexObj(&ind_texs[i][SPRITE_8BPP][align], (void*) &tex_swizzler_bin[tex_ofs], i, 4, GX_TF_IA8, GX_CLAMP, GX_REPEAT, GX_FALSE);
			GX_InitTexObjLOD(&ind_texs[i][SPRITE_8BPP][align], GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
			GX_InitTexPreloadRegion(&ind_regions[i][SPRITE_8BPP][align], 0x20000 + tex_ofs, size, 0, GX_TEXCACHE_NONE);
			GX_PreloadEntireTexture(&ind_texs[i][SPRITE_8BPP][align], &ind_regions[i][SPRITE_8BPP][align]);
			tex_ofs += size;
		}
	}

	for (u32 i = 1; i < 64; ++i) {
		u32 ii = i * 2;
		u32 size = ((ii >> 2) + ((ii & 0x3) > 0)) * 32;
		for (u32 align = 0; align < 4; ++align) {
			GX_InitTexObj(&ind_texs[i][SPRITE_16BPP][align], (void*) &tex_swizzler_bin[tex_ofs], i<<1, 4, GX_TF_IA8, GX_CLAMP, GX_REPEAT, GX_FALSE);
			GX_InitTexObjLOD(&ind_texs[i][SPRITE_16BPP][align], GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
			GX_InitTexPreloadRegion(&ind_regions[i][SPRITE_16BPP][align], 0x20000 + tex_ofs, size, 0, GX_TEXCACHE_NONE);
			GX_PreloadEntireTexture(&ind_texs[i][SPRITE_16BPP][align], &ind_regions[i][SPRITE_16BPP][align]);
			tex_ofs += size;
		}
	}

	GX_InitTexObj(&ind_celltex8, (void*) &cell8_tex, 2, 2, GX_TF_IA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
	GX_InitTexObjLOD(&ind_celltex8, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexPreloadRegion(&ind_cellreg8, 0x20000 + tex_ofs, 32, 0, GX_TEXCACHE_NONE);
	GX_PreloadEntireTexture(&ind_celltex8, &ind_cellreg8);
	tex_ofs += 32;

	f32 indmat8[2][3] = {
		{1.0f/2.0f, 0, 0},
		{0, 1.0f/16.0f, 0}
	};
	f32 indmat4[2][3] = {
		{1.0f/4.0f, 0, 0},
		{0, 1.0f/16.0f, 0}
	};
	GX_SetIndTexMatrix(GX_ITM_0, indmat8, 4);
	GX_SetIndTexMatrix(GX_ITM_1, indmat4, 4);
	f32 indmat_cell8[2][3] = {
		{1.0f/2.0f, 0, 0},
		{0, 1.0f/4.0f, 0}
	};
	GX_SetIndTexMatrix(GX_ITM_2, indmat_cell8, 4);

	GX_LOAD_XF_REGS(0x101F, 1); //Viewport FP
	wgPipe->F32 = 16777215.0f;

	memset(tlut_dirty, 1, sizeof(tlut_dirty));
}

void SGX_CellConverterSet(u32 cellsize, u32 bpp_id)
{
	if (cellsize & (bpp_id == SPRITE_8BPP)) {
		GX_LoadTexObjPreloaded(&ind_celltex8, &ind_cellreg8, GX_TEXMAP1);
		GX_SetNumIndStages(1);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
		GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_2, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_8, GX_ITS_4);
	}
	//TODO: Handle Cell 16BPP (1x1 and 2x2)
}


//Uses the sprite size (from 0 to 62) to make use of indirect textrue conversion
void SGX_SpriteConverterSet(u32 wsize, u32 bpp_id, u32 align)
{
	//XXX: Do something with the bpp_id
	if (bpp_id == SPRITE_16BPP) {
		GX_LoadTexObjPreloaded(&ind_texs[wsize][bpp_id][align], &ind_regions[wsize][bpp_id][align], GX_TEXMAP1);
		GX_SetNumIndStages(1);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
		GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_1, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_4, GX_ITS_1);
	} else {
		if (wsize > 1) {
			GX_LoadTexObjPreloaded(&ind_texs[wsize][bpp_id][align], &ind_regions[wsize][bpp_id][align], GX_TEXMAP1);
			GX_SetNumIndStages(1);
			GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
			GX_SetTevIndirect(GX_TEVSTAGE0, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_ST, GX_ITM_0, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE, GX_FALSE, GX_ITBA_OFF);
			GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_8, GX_ITS_1);
		} else {
			GX_SetNumIndStages(0);
			GX_SetTevDirect(GX_TEVSTAGE0);
		}
	}
}


//XXX: CHANGE THIS BACK TO THE WAY IT WAS BEFORE USING 4096 ENTRY TLUT
//XXX: We can skip all this computation and do this while changing vdp2s color ram
void SGX_ColorRamDirty(u32 pos)
{
	tlut_dirty[pos] = 1;
}


void SGX_InvalidateVRAM(void)
{
	GX_InvalidateTexRegion(&tex_region[0]);
	GX_InvalidateTexRegion(&tex_region[1]);
}

void SGX_TlutLoadCRAMImm(u32 pos, u32 trn_code, u32 size)
{
	GXTlutObj tlut;
	u16 *tlut_data = tlut_14bpp_ram + pos;
	if (trn_code) {
		if (size == GX_TLUT_16) {
			tlut_data = tlut_4bpp_ram + pos;
		} else {
			tlut_data = tlut_8bpp_ram + pos;
		}
	}
	GX_InitTlutObj(&tlut, tlut_data, GX_TL_RGB5A3, size << 4);
	if (size == GX_TLUT_16) {
		GX_LoadTlut(&tlut, TLUT_INDX_IMM4);
	} else {
		GX_LoadTlut(&tlut, TLUT_INDX_IMM8);
	}
}

void SGX_TlutCRAMUpdate(void)
{
	GXTlutObj tlut;

	//14bpp tlut
	volatile u32 *dst = (u32*) GX_RedirectWriteGatherPipe(tlut_14bpp_ram);
	u32 *src = (u32*) Vdp2ColorRam;
	for (u32 i = 0; i < 0x400; ++i) {
		*dst = (*(src++)) | 0x80008000;
	}
	GX_RestoreWriteGatherPipe();
	GX_InitTlutObj(&tlut, tlut_14bpp_ram, GX_TL_RGB5A3, 2048);
	GX_LoadTlut(&tlut , TLUT_INDX(TLUT_TYPE_FULL, 0));

	//8bpp tlut
	dst = (u32*) GX_RedirectWriteGatherPipe(tlut_8bpp_ram);
	src = (u32*) Vdp2ColorRam;
	for (u32 i = 0; i < 0x400; ++i) {
		if (i & 0x7F) {
			*dst = (*(src++)) | 0x80008000;
		} else {
			*dst = ((*(src++)) | 0x8000) & 0xFFFF;
		}
	}
	GX_RestoreWriteGatherPipe();
	GX_InitTlutObj(&tlut, tlut_8bpp_ram, GX_TL_RGB5A3, 2048);
	GX_LoadTlut(&tlut, TLUT_INDX(TLUT_TYPE_8BPP, 0));

	//4bpp tlut
	dst = (u32*) GX_RedirectWriteGatherPipe(tlut_4bpp_ram);
	src = (u32*) Vdp2ColorRam;
	for (u32 i = 0; i < 0x400; ++i) {
		if (i & 0x7) {
			*dst = (*(src++)) | 0x80008000;
		} else {
			*dst = ((*(src++)) | 0x8000) & 0xFFFF;
		}
	}
	GX_RestoreWriteGatherPipe();
	GX_InitTlutObj(&tlut, tlut_4bpp_ram, GX_TL_RGB5A3, 2048);
	GX_LoadTlut(&tlut, TLUT_INDX(TLUT_TYPE_4BPP, 0));
}




void SGX_SetZOffset(u32 offset)
{
	GX_LOAD_XF_REGS(0x101C, 1); //Set the Viewport Z
	wgPipe->F32 = (f32)(offset << 16);
}


void SGX_BeginVdp1(void)
{
	u32 tmem_even = 0x8C000000 | 0x100000 | 0x20000;	// 128k cache
	u32 tmem_odd = 0x90000000;	//No odd tmem cache

	u32 tex_filt = 0x80000000;
	u32 tex_lod = 0x84000000;
	//Modifiable values
	u32 tex_maddr = 0x94000000;
	u32 tex_size = 0x88000000;

	GX_LOAD_BP_REG(tex_filt);
	GX_LOAD_BP_REG(tex_lod);
	GX_LOAD_BP_REG(tex_size);
	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(tmem_odd);
	GX_LOAD_BP_REG(tex_maddr);
}


void SGX_SetTex(void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut)
{
	//Flush Texture State
	GX_LOAD_BP_REG(0x0F << 24);
	//Set texture address and size
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	u32 tex_size = 0x88000000 | (fmt << 20) | (((h-1) & 0x3FFu) << 10) | ((w-1) & 0x3FFu);
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tex_size);
	//If tlut is used set its address
	if (fmt > 7) {
		u32 tlut_addr = 0x98000800 | (tlut & 0x3ff);
		GX_LOAD_BP_REG(tlut_addr);
	}
	//Flush Texture State
	GX_LOAD_BP_REG(0x0F << 24);
}



void SGX_EndVdp1(void)
{

}


void SGX_BeginVdp2Scroll(u32 fmt, u32 sz)
{
	u32 tmem_even = 0x8C000000 | 0x100000 | 0x20000;	// 128k cache
	u32 tmem_odd = 0x90000000;	//No odd tmem cache

	u32 tex_filt = 0x80000000;
	u32 tex_lod = 0x84000000;
	//Modifiable values
	u32 tex_maddr = 0x94000000;
	u32 tex_size = 0x88000000 | (fmt << 20) | (((sz-1) & 0x3FFu) << 10) | ((sz-1) & 0x3FFu);

	GX_LOAD_BP_REG(tex_filt);
	GX_LOAD_BP_REG(tex_lod);
	GX_LOAD_BP_REG(tex_size);
	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(tmem_odd);
	GX_LOAD_BP_REG(tex_maddr);
}


void SGX_SetVdp2Texture(void *img_addr, u32 tlut)
{
	//Set texture address and size
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	GX_LOAD_BP_REG(tex_maddr);

	//If tlut is used set its address
	u32 tlut_addr = 0x98000800 | (tlut & 0x3ff);
	GX_LOAD_BP_REG(tlut_addr);
}
