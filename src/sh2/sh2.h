


//===================================
// SH2 Flags
//===================================
#define SH2_FLAG_SLAVE		0x01
#define SH2_FLAG_IDLE		0x02
#define SH2_FLAG_SLEEPING	0x04

//===================================
// On-Chip peripheral modules (Addresses 0xFFFFFE00 - 0xFFFFFFFF)
//===================================
#define OC_SMR			0x000
#define OC_BRR			0x001
#define OC_SCR			0x002
#define OC_TDR			0x003
#define OC_SSR			0x004
#define OC_RDR			0x005
#define OC_TIER			0x010
#define OC_FTCSR		0x011
#define OC_FRC			0x012
#define OC_OCRA			0x014
#define OC_OCRB			0x014	//WTF.. Add 0x10
#define OC_TRC			0x016
#define OC_TOCR			0x017
#define OC_FICR			0x018
#define OC_IPRB			0x060
#define OC_VCRA			0x062
#define OC_VCRB			0x064
#define OC_VCRC			0x066
#define OC_VCRD			0x068
#define OC_DRCR0		0x071
#define OC_DRCR1		0x072
#define OC_WTCSR		0x080
#define OC_WTCNT		0x081
#define OC_RSTCSR		0x082
#define OC_SBYCR		0x091
#define OC_ICR			0x0E0
#define OC_IPRA			0x0E2
#define OC_VCRWDT		0x0E4
#define OC_DVSR			0x100
#define OC_DVDNT		0x104
#define OC_DVCR			0x108
#define OC_VCRDIV		0x10C
#define OC_DVDNTH		0x110
#define OC_DVDNTL		0x114
#define OC_BARAH		0x140
#define OC_BARAL		0x142
#define OC_BAMRAH		0x144
#define OC_BAMRAL		0x146
#define OC_BBRA			0x148
#define OC_BARBH		0x160
#define OC_BARBL		0x162
#define OC_BAMRBH		0x164
#define OC_BAMRBL		0x166
#define OC_BBRB			0x168
#define OC_BDRBH		0x170
#define OC_BDRBL		0x172
#define OC_BDMRBH		0x174
#define OC_BDMRBL		0x176
#define OC_BRCR			0x178
#define OC_SAR0			0x180
#define OC_DAR0			0x184
#define OC_TCR0			0x188
#define OC_CHCR0		0x18C
#define OC_SAR1			0x190
#define OC_DAR1			0x194
#define OC_TCR1			0x198
#define OC_CHCR1		0x19C
#define OC_VCRDMA0		0x1A0
#define OC_VCRDMA1		0x1A8
#define OC_DMAOR		0x1B0
#define OC_BCR1			0x1E0
#define OC_BCR2			0x1E4
#define OC_WCR			0x1E8
#define OC_MCR			0x1EC
#define OC_RTCSR		0x1F0
#define OC_RTCNT		0x1F4
#define OC_RTCOR		0x1F8




