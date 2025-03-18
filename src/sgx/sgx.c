
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


//XXX: divide?
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
	GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
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

	//Set the color bank.
	tlut_data = (u32*) memalign(32, 0x200);
	u32 val = 0x00000001;
	u32 *dst = tlut_data;
	for (int i = 0; i < 128; ++i) {
		*dst = val;
		++dst;
		val += 0x00020002;
	}
	DCFlushRange(tlut_data, 0x200);
	SGX_LoadTlut(tlut_data, TLUT_SIZE_256 | TLUT_INDX_CLRBANK);
	SGX_Vdp1Init();
	SGX_Vdp2Init();
	SGX_CellConverterInit();

	GX_EnableTexOffsets(GX_TEXCOORD0, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD1, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD2, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD3, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD4, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD5, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD6, GX_ENABLE, GX_ENABLE);
	GX_EnableTexOffsets(GX_TEXCOORD7, GX_ENABLE, GX_ENABLE);
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

}





void SGX_SetZOffset(u32 offset)
{
	GX_LOAD_XF_REGS(0x101C, 1); //Set the Viewport Z
	wgPipe->F32 = (f32) (16777215 - offset);
}


void SGX_SetVtxOffset(f32 x, f32 y)
{
	f32 p0 = 2.0f / 640.0f;
	f32 p1 = -(640.0f + (x*2.0f) ) / 640.0f;
	f32 p2 = 2.0f / -480.0f;
	f32 p3 = -(480.0f + (y*2.0f)) / -480.0f;

	GX_LOAD_XF_REGS(0x1020, 4); //Set the Viewport Origin
	wgPipe->F32 = p0;
	wgPipe->F32 = p1;
	wgPipe->F32 = p2;
	wgPipe->F32 = p3;
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
	__SGX_FLUSH_TEX_STATE;
		GX_LOAD_BP_REG(tlut_addr);
		GX_LOAD_BP_REG(tlut_addr_conf);
	__SGX_FLUSH_TEX_STATE;
}


void SGX_PreloadTex(const void *tex_addr, u32 tmem_addr, u32 tile_cnt_fmt)
{
	u32 tex_maddr = 0x60000000 | (MEM_VIRTUAL_TO_PHYSICAL(tex_addr) >> 5);
	u32 tmem_even = 0x61000000 | (tmem_addr & 0x7fff);
	u32 tex_size = 0x63000000 | (tile_cnt_fmt & 0x1ffff);

	__SGX_FLUSH_TEX_STATE;
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(0x62000000);
	GX_LOAD_BP_REG(tex_size);
	__SGX_FLUSH_TEX_STATE;
}


void SGX_SetTexPreloaded(u32 mapid, SGXTexPre *tex)
{
	mapid = (((mapid & 0x4) << 3) |(mapid & 0x3)) << 24;
	u32 tex_filt = 0x80000000 | mapid | tex->attr;
	u32 tex_lod = 0x84000000 | mapid;
	u32 tex_size = 0x88000000 | mapid | tex->fmt;

	u32 tmem_even = 0x8C000000 | mapid | tex->addr;
	u32 tmem_odd = 0x90000000 | mapid;	//No odd tmem cache

	__SGX_FLUSH_TEX_STATE;
	GX_LOAD_BP_REG(tex_filt);
	GX_LOAD_BP_REG(tex_lod);
	GX_LOAD_BP_REG(tex_size);

	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(tmem_odd);
	__SGX_FLUSH_TEX_STATE;
}

void SGX_SetTex(void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut)
{
	//Flush Texture State
	__SGX_FLUSH_TEX_STATE;
	//Set texture address and size
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	u32 tex_size = 0x88000000 | (fmt << 20) | (((h-1) & 0x3FFu) << 10) | ((w-1) & 0x3FFu);
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tex_size);
	//If tlut is used set its address
	if (fmt > 7) {
		u32 tlut_addr = 0x98000000 | (tlut & 0xfff);
		GX_LOAD_BP_REG(tlut_addr);
	}
	//Flush Texture State
	__SGX_FLUSH_TEX_STATE;
}

void SGX_SetOtherTex(u32 mapid, void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut)
{
	//Flush Texture State
	__SGX_FLUSH_TEX_STATE;
	//Set texture address and size
	mapid <<= 24;
	u32 tex_maddr = 0x94000000 | mapid | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	u32 tex_size = 0x88000000 | mapid | (fmt << 20) | (((h-1) & 0x3FFu) << 10) | ((w-1) & 0x3FFu);
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tex_size);
	//If tlut is used set its address
	if (fmt > 7) {
		u32 tlut_addr = 0x98000000 | mapid | (tlut & 0xfff);
		GX_LOAD_BP_REG(tlut_addr);
	}
	//Flush Texture State
	__SGX_FLUSH_TEX_STATE;
}



void SGX_EndVdp1(void)
{

}


void SGX_BeginVdp2Scroll(u32 fmt, u32 sz)
{
	u32 tmem_even = 0x8C000000 | 0x100000 | 0x20000;	// 128k cache
	u32 tmem_odd = 0x90000000;	//No odd tmem cache

	u32 tex_filt = 0x8000000A; //Wrap s/t = mirror
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
	__SGX_FLUSH_TEX_STATE;
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(img_addr) >> 5);
	GX_LOAD_BP_REG(tex_maddr);
	__SGX_FLUSH_TEX_STATE;
	//If tlut is used set its address
	//u32 tlut_addr = 0x98000800 | (tlut & 0x3ff);
	//GX_LOAD_BP_REG(tlut_addr);
}


// GX replacement functions (direct register access)
void SGX_Begin(u8 primitive, u8 vtxfmt, u16 vtxcnt)
{
	wgPipe->U8 = primitive | (vtxfmt & 0x7);
	wgPipe->U16 = vtxcnt;
}

void SGX_End() {/*Does nothing*/}

void SGX_SetTevGens(u8 num_tevs, u8 num_texgens, u8 num_chans)
{
	//gen_mode register:
	//[0x00][19 : coplanar_en][16-18 : num_indtevs][14-15 : cull_mode][10-13 : num_tevs][9 : multsamp_en][4-6 : num_chans][0-3 : num_texgens]
	num_tevs = (num_tevs-1) & 0xF;
	num_texgens &= 0xF;
	num_chans &= 0x7;
	u32 genmode = (0x00 << 24) | (num_texgens) | (num_chans << 4) | (num_tevs << 10);

	SGX_MASK_BP(0x3C7Fu); //Only write set bits
	GX_LOAD_BP_REG(genmode);
	GX_LOAD_XF_REG(0x103F, num_texgens);
	GX_LOAD_XF_REG(0x103F, num_chans);
}

void SGX_SetTevGensExt(u8 num_tevs, u8 num_texgens, u8 num_chans, u8 num_indtevs)
{
	num_tevs = (num_tevs-1) & 0xF;
	num_texgens &= 0xF;
	num_chans &= 0x7;
	num_indtevs &= 0x7;
	u32 genmode = (0x00 << 24) | (num_texgens) | (num_chans << 4) | (num_tevs << 10) | (num_indtevs << 16);

	SGX_MASK_BP(0x73C7Fu); //Only write set bits
	GX_LOAD_BP_REG(genmode);
	GX_LOAD_XF_REG(0x103F, num_texgens);
	GX_LOAD_XF_REG(0x103F, num_chans);
}

void SGX_SetNumIndStages(u8 num_indtevs)
{
	SGX_MASK_BP(0x70000u); //Only write to num_indtevs bits
	GX_LOAD_BP_REG(((num_indtevs & 0x7) << 16));
}

void SGX_SetCullMode(u8 mode)
{
	mode = ((mode>>1) | (mode << 1)) & 0x3;
	SGX_MASK_BP(0xC000);
	GX_LOAD_BP_REG(mode);
}


void SGX_SetCurrentMtxLo(u32 posn, u32 t0, u32 t1, u32 t2, u32 t3)
{
	u32 mtx = (posn & 0x3F) | ((t0 & 0x3F) << 6) | ((t1 & 0x3F) << 12) | ((t2 & 0x3F) << 18) | ((t3 & 0x3F) << 24);
	GX_LOAD_CP_REG(0x30, mtx);
	GX_LOAD_XF_REG(0x1018, mtx);
}

void SGX_SetCurrentMtxHi(u32 t4, u32 t5, u32 t6, u32 t7)
{
	u32 mtx = (t4 & 0x3F) | ((t5 & 0x3F) << 6) | ((t6 & 0x3F) << 12) | ((t7 & 0x3F) << 18);
	GX_LOAD_CP_REG(0x40, mtx);
	GX_LOAD_XF_REG(0x1019, mtx);
}

