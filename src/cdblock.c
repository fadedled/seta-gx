#if 0
#include "cdblock.h"

struct CdBlock {
	u16 cr[4];
	u8 status;
	u8 auth;
} cdb;


void cdb_Init(char *filename) {


	cdb.auth = 0;
}

void cdb_Deinit(char *filename) {

}


void cdb_SetInterrupt(u32 irq) {

}


u8 cdb_Read8(u32 addr)
{
	switch (addr) {
		case 0x5895019: return 0x30;
		case 0x589501D: return 0x00;
	}
}

u16 cdb_Read16(u32 addr)
{
	addr &= 0x3E;

	switch (addr) {
		case 0x00: return DoReadTransfer();
		case 0x02: return DoReadTransfer();
		case 0x08: return cdb.hirq;
		case 0x0C: return cdb.hirqmask;
		case 0x18: return cdb.rr[0];
		case 0x1C: return cdb.rr[1];
		case 0x20: return cdb.rr[2];
		case 0x24:
			proc_command = 0;
			periodic_reports_ready = 1;
		return cdb.rr[3];
		default: //Unhandled
	}
}

void cdb_Write16(u32 addr, u32 val)
{
	addr &= 0x3E;

	switch (addr) {
		case 0x00: DoWriteTransfer(val); break;
		case 0x02: DoWriteTransfer(val); break;
		case 0x08: cdb.hirq &= val; _updateInterrupts(); break;
		case 0x0C: cdb.hirqmask = val; _updateInterrupts(); break;
		case 0x18: cdb.cr[0]; proc_command = 1; break;
		case 0x1C: cdb.cr[1]; break;
		case 0x20: cdb.cr[2]; break;
		case 0x24: cdb.cr[3]; _setupCommand(); break;
		default: //Unhandled
	}
}

static void __ReportCDStatus(u8 status)
{
	cbd.rr[0] = (status << 8) | (cdb.flags << 4) | (cdb.repeatCnt);
	cbd.rr[1] = (cdb.ctrl_addr << 8) | cdb.track;
	cbd.rr[2] = (cdb.index << 8) | ((cdb.frame_addr >> 16) & 0xFF);
	cbd.rr[3] = cdb.drame_addr & 0xFFFF;
}


static u32 __getStatusCode(void)
{
	if (play_pending && cdb.status == STATUS_CODE_PAUSE) {
		return STATUS_CODE_PLAY;
	}
	return cdb.status;
}

static void __openTray(void)
{
	if (__getStatusCode != STATUS_CODE_OPEN) {
		cdb.status = STATUS_CODE_OPEN;
		cdb.disc_auth = 0;
		cdb.target_drive_cycles = DRIVE_CYCLES_NOT_PLAYING;
		cdb_SetInterrupt(CDB_HIRQ_DCHG | CDB_HIRQ_EFLS);
	}
}

static void cmd_GetStatus(void)
{
	__ReportCDStatus(__getStatusCode());
	cdb_SetInterrupt(CDB_HIRQ_CMOK);
}

static void cmd_GetHardwareInfo(void)
{
	cbd.cr[0] = __getStatusCode() << 8;
	cbd.cr[1] = 0x0002;
	cbd.cr[2] = 0x0000;
	cbd.cr[3] = 0x0600;

	cdb_SetInterrupt(CDB_HIRQ_CMOK);
}

static void cmd_GetTOC(void)
{

}

static void cmd_GetSessionInfo(void)
{

}

static void cmd_InitCDSystem(void)
{

}

static void cmd_OpenTray(void)
{
	__openTray();
	__ReportCDStatus();
	cdb_SetInterrupt(CDB_HIRQ_CMOK | CDB_HIRQ_EFLS | CDB_HIRQ_DCHG);
}

static void cmd_EndDataTransfer(void)
{

}

static void cmd_PlayDisc(void)
{

}

static void cmd_SeekDisc(void)
{

}

static void cmd_ScanDisc(void)
{

}

static void cmd_GetSubCodeQRW(void)
{

}

static void cmd_SetCDDeviceConn(void)
{

}

static void cmd_GetCDDeviceConn(void)
{

}

static void cmd_GetLastBufferDest(void)
{

}

static void cmd_SetFilterRange(void)
{

}

static void cmd_GetFilterRange(void)
{

}

static void cmd_SetFilterSubheaderCond(void)
{

}

static void cmd_GetFilterSubheaderCond(void)
{

}

static void cmd_GetFilterMode(void)
{

}

static void cmd_GetFilterMode(void)
{

}

static void cmd_GetFilterConn(void)
{

}

static void cmd_GetFilterConn(void)
{

}

static void cmd_ResetSelector(void)
{

}

static void cmd_getBuferSize(void)
{

}

static void cmd_getSectorNumber(void)
{

}

static void cmd_calcActualSize(void)
{

}

static void cmd_getActualSize(void)
{

}

static void cmd_getSectorInfo(void)
{

}

static void cmd_execFADSearch(void)
{

}

static void cmd_getFADSearchResult(void)
{

}

static void cmd_getSectorLength(void)
{

}

static void cmd_getSectorData(void)
{

}

static void cmd_deleteSectorData(void)
{

}

static void cmd_getThenDeleteSectorData(void)
{

}

static void cmd_putSectorData(void)
{

}

static void cmd_copySectorData(void)
{

}

static void cmd_moveSectorData(void)
{

}

static void cmd_getCopyError(void)
{

}

static void cmd_changeDirectory(void)
{

}

static void cmd_readDirectory(void)
{

}

static void cmd_getFileSystemScope(void)
{

}

static void cmd_getFileInfo(void)
{

}

static void cmd_readFile(void)
{

}

static void cmd_abortFile(void)
{

}

static void cmd_MpegInit(void)
{
	cbd.rr[0] = 0xFF00;
	cbd.rr[1] = 0x0000;
	cbd.rr[2] = 0x0000;
	cbd.rr[3] = 0x0000;
	cdb_SetInterrupt(HIRQ_CMOK | HIRQ_MPED | HIRQ_MPST);
}

static void cmd_authDevice(void)
{
	switch (cbd.cr[1]) {
		case 0: cdb.auth |= 4; cdb_SetInterrupt(HIRQ_EFLS | HIRQ_CSCT); brake;
		case 1: cdb.auth |= 2; cdb_SetInterrupt(HIRQ_MPED); brake;
	}

	cdb.drive_cycles = DRIVE_CYCLES_NOT_PLAYING;
	cdb.status = STATUS_CODE_PAUSE;
	__ReportCDStatus(cdb.status);
	cdb_SetInterrupt(HIRQ_CMOK);
}

static void cmd_isDeviceAuth(void)
{
	cbd.rr[0] = __getStatusCode() << 8;
	cbd.rr[1] = (cbd.cr[1] ? cdb.auth & 2 : cdb.auth & 4);
	cbd.rr[2] = 0x0000;
	cbd.rr[3] = 0x0000;

	cdb_SetInterrupt(HIRQ_CMOK);
}


void cdb_ExecuteCommand(void)
{
	u32 cmd = (Cs2Area->reg.CR1 >> 8) & 0xFF;
	switch (cmd) {
		case 0x00: cmd_getStatus();			break; // getStatus
		case 0x01: cmd_getHardwareInfo();		break; // getHardwareInfo
		case 0x02: cmd_getTOC();				break; // getTOC
		case 0x03: cmd_getSessionInfo();		break; // getSessionInfo
		case 0x04: cmd_initCDSystem();		break; // initCDSystem
		case 0x05: cmd_openTray();			break; // openTray
		case 0x06: cmd_endDataTransfer();		break; // endDataTransfer
		case 0x10: cmd_playDisc();			break; // playDisc
		case 0x11: cmd_seekDisc();			break; // seekDisc
		case 0x12: cmd_scanDisc();			break; // scanDisc
		case 0x20: cmd_getSubCodeQRW();		break; // getSubCodeQRW
		case 0x30: cmd_setCDDeviceConn();		break; // setCDDeviceConn
		case 0x31: cmd_getCDDeviceConn();		break; // getCDDeviceConn
		case 0x32: cmd_getLastBufferDest();	break; // getLastBufferDest
		case 0x40: cmd_setFilterRange();		break; // setFilterRange
		case 0x41: cmd_getFilterRange();		break; // getFilterRange
		case 0x42: cmd_setFilterSubheaderCond();	break; // setFilterSubheaderCond
		case 0x43: cmd_getFilterSubheaderCond();	break; // getFilterSubheaderCond
		case 0x44: cmd_setFilterMode();		break; // setFilterMode
		case 0x45: cmd_getFilterMode();		break; // getFilterMode
		case 0x46: cmd_setFilterConn();		break; // setFilterConn
		case 0x47: cmd_getFilterConn();		break; // getFilterConn
		case 0x48: cmd_resetSelector();		break; // resetSelector
		case 0x50: cmd_getBuferSize();		break; // getBuferSize
		case 0x51: cmd_getSectorNumber();		break; // getSectorNumber
		case 0x52: cmd_calcActualSize();		break; // calcActualSize
		case 0x53: cmd_getActualSize();		break; // getActualSize
		case 0x54: cmd_getSectorInfo();		break; // getSectorInfo
		case 0x55: cmd_execFADSearch();		break; // execFADSearch
		case 0x56: cmd_getFADSearchResult();	break; // getFADSearchResult
		case 0x60: cmd_getSectorLength();		break; // getSectorLength
		case 0x61: cmd_getSectorData();		break; // getSectorData
		case 0x62: cmd_deleteSectorData();	break; // deleteSectorData
		case 0x63: cmd_getThenDeleteSectorData();	break; // getThenDeleteSectorData
		case 0x64: cmd_putSectorData();		break; // putSectorData
		case 0x65: cmd_copySectorData();		break; // copySectorData
		case 0x66: cmd_moveSectorData();		break; // moveSectorData
		case 0x67: cmd_getCopyError();		break; // getCopyError
		case 0x70: cmd_changeDirectory();		break; // changeDirectory
		case 0x71: cmd_readDirectory();		break; // readDirectory
		case 0x72: cmd_getFileSystemScope();	break; // getFileSystemScope
		case 0x73: cmd_getFileInfo();			break; // getFileInfo
		case 0x74: cmd_readFile();			break; // readFile
		case 0x75: cmd_abortFile();			break; // abortFile
		//MPEG
		case 0x90: break; // MpegGetStatus
		case 0x91: break; // MpegGetInterrupt
		case 0x92: break;
		case 0x93: cmd_MpegInit();			break;
		case 0x94: break;
		case 0x95: break;
		case 0x96: break;
		case 0x9A: break;
		case 0x9B: break;
		case 0x9D: break;
		case 0x9E: break;
		case 0xA0: break;
		case 0xA1: break;
		case 0xA2: break;
		case 0xA3: break;
		case 0xA4: break;
		case 0xAF: break;
		//
		case 0xE0: cmd_authDevice(); break; // authDevice
		case 0xE1: cmd_isDeviceAuth(); break; // isDeviceAuthenticated
		case 0xE2: break; // getMpegRom
		default: break; //
	}
}
#endif