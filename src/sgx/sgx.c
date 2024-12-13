
#include "sgx.h"
#include "../vidshared.h"
#include "../vdp1.h"
#include "../vdp2.h"
#include "../osd/gui.h"
#include <malloc.h>

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


#define __FLUSH_TEX_STATE		GX_LOAD_BP_REG(0x0F << 24)


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
static GXTexRegion ind_cellreg8;
static GXTexObj    ind_celltex8;

u32 *tlut_data;
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
#if USE_NEW_VDP1
	ptr->tmem_addr_conf = 0x65000000 | TLUT_INDX_CLRBANK;
#else
	ptr->tmem_addr_conf = 0x65000000 | idx;
#endif
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

	//Set matrices
	Mtx ident_mtx;
	guMtxIdentity(ident_mtx);
	GX_LoadPosMtxImm(ident_mtx, GXMTX_IDENTITY);
	guMtxScale(ident_mtx, 2.0f, 2.0f, 1.0f);
	GX_LoadPosMtxImm(ident_mtx, GXMTX_IDENTITY_2X);
	GX_SetCurrentMtx(GXMTX_IDENTITY);


	//Set the texcache regions
	GX_SetTexRegionCallback(__SGX_CalcTexRegion);
	GX_SetTlutRegionCallback(__SGX_CalcTlutRegion);

	f32 indmat8[2][3] = {
		{1.0f/4.0f, 0, 0},
		{0, 1.0f/32.0f, 0}
	};
	f32 indmat4[2][3] = {
		{1.0f/8.0f, 0, 0},
		{0, 1.0f/32.0f, 0}
	};
	GX_SetIndTexMatrix(GX_ITM_0, indmat8, 5);
	GX_SetIndTexMatrix(GX_ITM_1, indmat4, 5);
	f32 indmat_cell8[2][3] = {
		{1.0f/2.0f, 0, 0},
		{0, 1.0f/4.0f, 0}
	};
	GX_SetIndTexMatrix(GX_ITM_2, indmat_cell8, 4);

	GX_LOAD_XF_REGS(0x101F, 1); //Viewport FP
	wgPipe->F32 = 16777215.0f;

	memset(tlut_dirty, 1, sizeof(tlut_dirty));

	//Set the color bank.
	tlut_data = (u32*) memalign(32, 0x200);
	u32 val = 0x70007001;
	u32 *dst = tlut_data;
	for (int i = 0; i < 128; ++i) {
		*dst = val;
		++dst;
		val += 0x00020002;
	}
	*tlut_data = 0x00007001;
	DCFlushRange(tlut_data, 0x200);
	SGX_Vdp1Init();
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
	wgPipe->F32 = (f32) (16777215 - offset);
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

void SGX_InitTex(u32 mapid, u32 even, u32 odd)
{
	//Texture regions
	mapid <<= 24;
	u32 tmem_even = 0x8C000000 | mapid | even;	// 128k cache
	u32 tmem_odd = 0x90000000 | mapid;	//No odd tmem cache
	//Texture filter
	u32 tex_filt = 0x80000000 | mapid | odd;
	u32 tex_lod = 0x84000000 | mapid;

	GX_LOAD_BP_REG(tex_filt);
	GX_LOAD_BP_REG(tex_lod);
	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(tmem_odd);
}

void SGX_InitTexSet(u32 tex_count, u32 map0_even, u32 map1_even)
{
#if 0
	u32 cache_regions[4] = {
		0x120000, 0xD8000 | (0x20000 >> 5), 0xD8000 | (0x28000 >> 5), 0x120000
	};
	//Texture regions
	u32 reg = cache_regions[mapid];
	i <<= 24;
	for (u32 i = 0; i < tex_count; ++i) {
		u32 tmem_even = 0x8C000000 | mapid | reg;	// 128k cache
		u32 tmem_odd = 0x90000000 | mapid;	//No odd tmem cache
		//Texture filter
		u32 tex_filt = 0x80000000 | mapid;
		u32 tex_lod = 0x84000000 | mapid;

		GX_LOAD_BP_REG(tex_filt);
		GX_LOAD_BP_REG(tex_lod);
		GX_LOAD_BP_REG(tmem_even);
		GX_LOAD_BP_REG(tmem_odd);
	}
#endif
}

void SGX_LoadTlut(void *data_addr, u32 tlut)
{
	u32 tlut_addr = 0x64000000 | (MEM_VIRTUAL_TO_PHYSICAL(data_addr) >> 5);
	u32 tlut_addr_conf = 0x65000000 | tlut;
	__FLUSH_TEX_STATE;
		GX_LOAD_BP_REG(tlut_addr);
		GX_LOAD_BP_REG(tlut_addr_conf);
	__FLUSH_TEX_STATE;
}


void SGX_PreloadTex(void *tex_addr, u32 tmem_addr, u32 tile_cnt_fmt)
{
	u32 tex_maddr = 0x60000000 | (MEM_VIRTUAL_TO_PHYSICAL(tex_addr) >> 5);
	u32 tmem_even = 0x61000000 | (tmem_addr & 0x7fff);
	u32 tex_size = 0x63000000 | (tile_cnt_fmt & 0x1ffff);

	__FLUSH_TEX_STATE;
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(0x62000000);
	GX_LOAD_BP_REG(tex_size);
	__FLUSH_TEX_STATE;
}


void SGX_SetTexPreloaded(u32 mapid, SGXTexPre *tex)
{
	mapid = (((mapid & 0x4) << 3) |(mapid & 0x3)) << 24;
	u32 tex_filt = 0x80000000 | mapid | tex->attr;
	u32 tex_lod = 0x84000000 | mapid;
	u32 tex_size = 0x88000000 | mapid | tex->fmt;

	u32 tmem_even = 0x8C000000 | mapid | tex->addr;
	u32 tmem_odd = 0x90000000 | mapid;	//No odd tmem cache

	__FLUSH_TEX_STATE;
	GX_LOAD_BP_REG(tex_filt);
	GX_LOAD_BP_REG(tex_lod);
	GX_LOAD_BP_REG(tex_size);

	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(tmem_odd);
	__FLUSH_TEX_STATE;
}

void SGX_SetTex(void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut)
{
	//Flush Texture State
	__FLUSH_TEX_STATE;
	//Set texture address and size
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	u32 tex_size = 0x88000000 | (fmt << 20) | (((h-1) & 0x3FFu) << 10) | ((w-1) & 0x3FFu);
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tex_size);
	//If tlut is used set its address
	if (fmt > 7) {
		u32 tlut_addr = 0x98000000 | (GX_TL_RGB5A3 << 10) | (tlut & 0x3ff);
		GX_LOAD_BP_REG(tlut_addr);
	}
	//Flush Texture State
	__FLUSH_TEX_STATE;
}

void SGX_SetOtherTex(u32 mapid, void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut)
{
	//Flush Texture State
	__FLUSH_TEX_STATE;
	//Set texture address and size
	mapid <<= 24;
	u32 tex_maddr = 0x94000000 | mapid | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	u32 tex_size = 0x88000000 | mapid | (fmt << 20) | (((h-1) & 0x3FFu) << 10) | ((w-1) & 0x3FFu);
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tex_size);
	//If tlut is used set its address
	if (fmt > 7) {
		u32 tlut_addr = 0x98000000 | mapid | (GX_TL_RGB5A3 << 10) | (tlut & 0x3ff);
		GX_LOAD_BP_REG(tlut_addr);
	}
	//Flush Texture State
	__FLUSH_TEX_STATE;
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
	__FLUSH_TEX_STATE;
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	GX_LOAD_BP_REG(tex_maddr);
	__FLUSH_TEX_STATE;
	//If tlut is used set its address
	//u32 tlut_addr = 0x98000800 | (tlut & 0x3ff);
	//GX_LOAD_BP_REG(tlut_addr);
}


