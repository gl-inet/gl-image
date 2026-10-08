#
# Copyright (c) 2024, MediaTek Inc. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
ifneq (,$(filter 1, $(FSEK) $(FW_ENC) $(FIP_ENC)))
PLAT_INCLUDES		+= -I$(APSOC_COMMON)/fw_dec/mtk_roe

PREBUILT_LIBS		+= $(APSOC_COMMON)/fw_dec/mtk_roe/release/mtk_roe.o
endif
