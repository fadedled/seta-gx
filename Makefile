#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
# prevent deletion of implicit targets
#---------------------------------------------------------------------------------
.SECONDARY:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPRO)/libogc2/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	seta-gx
BUILD		:=	build
SOURCES		:=	src \
			src/c68k \
			src/q68 \
			src/osd \
			src/drc \
			src/sgx 
#			src/sh2/mame
			
DATA		:=	#res
INCLUDES	:=	

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
DDEFINES= -DUSE_WIIGX -DWIICONVERTASM 
#DDEFINES+= -Dmain=SDL_main
DDEFINES+= -DHW_RVL
DDEFINES+= -DDRC_SH2
DDEFINES+= -DEXEC_FROM_CACHE
DDEFINES+= -DOPTIMIZED_DMA
DDEFINES+= -DHAVE_Q68
DDEFINES+= -DQ68_DISABLE_ADDRESS_ERROR
DDEFINES+= -DUSE_SCSP2
DDEFINES+= -DSCSP_PLUGIN
DDEFINES+= -DWORDS_BIGENDIAN
DDEFINES+= -DAUTOLOADPLUGIN
DDEFINES+= -DHAVE_STRCASECMP

VDEFINES=-DPACKAGE=\"saturn-gx\" -DVERSION=\"r2926\" -DWIIVERSION=\"ver.\ 1.0\"  -DREENTRANT_SYSCALLS_PROVIDED

MACHDEP = -DGEKKO -mrvl -mcpu=750 -meabi -mhard-float -fsigned-char -ffast-math -funroll-loops -fauto-inc-dec -finline-functions #-fomit-frame-pointer

#This is only for profiling with gperf
MACHDEP += #-fomit-frame-pointer
CFLAGS	= -g -O2 -Wall -static -falign-functions=2 $(MACHDEP) $(DDEFINES) $(VDEFINES) $(INCLUDE)
#CFLAGS =  $(DDEFINES) -g -Ofast -mrvl -Wall $(MACHDEP) -$(INDLUDE)
CXXFLAGS	=	$(CFLAGS)

LDFLAGS	= -g $(MACHDEP) -mrvl -Wl,-Map,$(notdir $@).map -static


#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= -laesnd -lfat -lwiiuse -lbte -logc -lchdr -llzma -lzstd -lm -lz

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) 

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(foreach dir,$(TEXTURES),$(CURDIR)/$(dir))
					
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
SCFFILES	:=	$(foreach dir,$(TEXTURES),$(notdir $(wildcard $(dir)/*.scf)))
TPLFILES	:=	$(SCFFILES:.scf=.tpl)

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) $(addsuffix .o,$(TPLFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES) m68kops.o m68kcpu.o m68kopdm.o m68kopac.o m68kopdm.o m68kopnz.o

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES))) $(addsuffix .h,$(subst .,_,$(TPLFILES)))

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBOGC_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: $(BUILD) clean

#---------------------------------------------------------------------------------
$(BUILD):
	@echo $(INCLUDE) $(LIBPATHS)
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	cp $(OUTPUT).dol SetaGX/boot.dol
	size -A -d $(OUTPUT).elf | grep -w  --color .text


#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol
	
#---------------------------------------------------------------------------------
run:
	wiiload $(TARGET).dol


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin extension
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

c68k/c68kexec.o:
	$(CC) $(VDEFINES) $(DDEFINES) -mrvl -Wall $(MACHDEP) -I$(LIBOGC_INC) -c c68k/c68kexec.c -o $@ 


m68kops.o: 
	$(CC) $(VDEFINES) $(DDEFINES) -mrvl -Wall $(MACHDEP) -I$(LIBOGC_INC) -c $(CURDIR)/../src/musashi/m68kops.c -o $@ 
	
m68kcpu.o: 
	$(CC) $(VDEFINES) $(DDEFINES) -mrvl -Wall $(MACHDEP) -I$(LIBOGC_INC) -c $(CURDIR)/../src/musashi/m68kcpu.c -o $@ 

m68kopac.o: 
	$(CC) $(VDEFINES) $(DDEFINES) -mrvl -Wall $(MACHDEP) -I$(LIBOGC_INC) -c $(CURDIR)/../src/musashi/m68kopac.c -o $@ 

m68kopdm.o: 
	$(CC) $(VDEFINES) $(DDEFINES) -mrvl -Wall $(MACHDEP) -I$(LIBOGC_INC) -c $(CURDIR)/../src/musashi/m68kopdm.c -o $@ 
	
m68kopnz.o: 
	$(CC) $(VDEFINES) $(DDEFINES) -mrvl -Wall $(MACHDEP) -I$(LIBOGC_INC) -c $(CURDIR)/../src/musashi/m68kopnz.c -o $@ 



-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
