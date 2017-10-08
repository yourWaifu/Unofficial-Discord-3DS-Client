#cd ..\..\Users\steve\Documents\nsprojects\sleepy_discord\test\test_3ds

# TARGET #

TARGET := 3DS
LIBRARY := 0

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    ifeq ($(strip $(DEVKITPRO)),)
        $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
    endif
endif

# COMMON CONFIGURATION #

NAME := White Haired Cat Girl

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include ../.. ../../sleepy_discord/IncludeNonexistent $(DEVKITPRO)/examples/3ds/graphics/printing/system-font/source
SOURCE_DIRS := source

EXTRA_OUTPUT_FILES :=

BUILD_FLAGS :=
BUILD_FLAGS_CC :=
BUILD_FLAGS_CXX := -g -Og -DSLEEPY_ONE_THREAD -DSLEEPY_CUSTOM_SESSION
RUN_FLAGS := 

# 3DS/Wii U CONFIGURATION #

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    TITLE := White Haired Cat Girl
    DESCRIPTION := A Discord bot
    AUTHOR := Sleepy Flower Girl
endif

# 3DS CONFIGURATION #

ifeq ($(TARGET),3DS)
    LIBRARY_DIRS += $(DEVKITPRO)/libctru $(DEVKITPRO)/portlibs/3ds
    LIBRARIES += citro3d ctru wslay sleepy_discord

    PRODUCT_CODE := CTR-P-WHCG
    UNIQUE_ID := 0xD5CDB

    CATEGORY := Application
    USE_ON_SD := true

    MEMORY_TYPE := Application
    SYSTEM_MODE := 64MB
    SYSTEM_MODE_EXT := 124MB
    CPU_SPEED := 804MHz
    ENABLE_L2_CACHE := true

    ICON_FLAGS :=

    ROMFS_DIR := romfs
    BANNER_AUDIO := meta/audio_3ds.wav
    BANNER_IMAGE := meta/banner_3ds.png
    ICON := meta/icon_3ds.png
endif

# INTERNAL #

include buildtools/make_base