OE_SDK_PATH=../../packages/open-enclave-cross.0.8.1-c3b6262c-1
OE_SDK_INC_PATH=$(OE_SDK_PATH)/build/native/include
OEEDGER8R=$(OE_SDK_PATH)/tools/oeedger8r

CFLAGS += $(EXTRA_CFLAGS)

CFLAGS +=                              \
    -I..                               \
    -I$(OE_SDK_INC_PATH)/new_platforms \
    -I$(OE_SDK_INC_PATH)

CFLAGS += -DLINUX -DOE_USE_OPTEE

libdirs += $OELibPath$

../$projectname$_t.c: ../$projectname$.edl
	$(OEEDGER8R) --trusted --trusted-dir .. --search-path "$(OE_SDK_INC_PATH)" ../$projectname$.edl

../$projectname$_t.h: ../$projectname$.edl
	$(OEEDGER8R) --trusted --trusted-dir .. --search-path "$(OE_SDK_INC_PATH)" ../$projectname$.edl

# Add the c file generated from your EDL file here
srcs-y             += ../$projectname$_t.c

# Add additional sources here
srcs-y             += ../$projectname$_ecalls.c

libnames           += oeenclave
libnames           += oestdio_enc

# Add additional libraries here
# libnames         += ...
