/*
 * Copyright 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __VDPU34X_H264D_H__
#define __VDPU34X_H264D_H__

#include "vdpu34x_com.h"

/* base: OFFSET_CODEC_PARAMS_REGS */
typedef struct Vdpu34xRegH264dParam_t {
    struct SWREG64_H26X_SET {
        RK_U32      h26x_frame_orslice      : 1;
        RK_U32      h26x_rps_mode           : 1;
        RK_U32      h26x_stream_mode        : 1;
        RK_U32      h26x_stream_lastpacket  : 1;
        RK_U32      h264_firstslice_flag    : 1;
        RK_U32      reserve                 : 27;
    } h26x_set;

    struct SWREG65_CUR_POC {
        RK_U32      cur_top_poc : 32;
    } cur_poc;

    struct SWREG66_H264_CUR_POC1 {
        RK_U32      cur_bot_poc : 32;
    } cur_poc1;

    struct SWREG67_98_H264_REF_POC {
        RK_U32      ref_poc : 32;
    } ref0_31_poc[32];

    struct SWREG99_H264_REG0_3_INFO {

        RK_U32      ref0_field              : 1;
        RK_U32      ref0_topfield_used      : 1;
        RK_U32      ref0_botfield_used      : 1;
        RK_U32      ref0_colmv_use_flag     : 1;
        RK_U32      ref0_reserve            : 4;

        RK_U32      ref1_field              : 1;
        RK_U32      ref1_topfield_used      : 1;
        RK_U32      ref1_botfield_used      : 1;
        RK_U32      ref1_colmv_use_flag     : 1;
        RK_U32      ref1_reserve            : 4;

        RK_U32      ref2_field              : 1;
        RK_U32      ref2_topfield_used      : 1;
        RK_U32      ref2_botfield_used      : 1;
        RK_U32      ref2_colmv_use_flag     : 1;
        RK_U32      ref2_reserve            : 4;

        RK_U32      ref3_field              : 1;
        RK_U32      ref3_topfield_used      : 1;
        RK_U32      ref3_botfield_used      : 1;
        RK_U32      ref3_colmv_use_flag     : 1;
        RK_U32      ref3_reserve            : 4;
    } ref0_3_info;

    struct SWREG100_H264_REG4_7_INFO {

        RK_U32      ref4_field              : 1;
        RK_U32      ref4_topfield_used      : 1;
        RK_U32      ref4_botfield_used      : 1;
        RK_U32      ref4_colmv_use_flag     : 1;
        RK_U32      ref4_reserve            : 4;

        RK_U32      ref5_field              : 1;
        RK_U32      ref5_topfield_used      : 1;
        RK_U32      ref5_botfield_used      : 1;
        RK_U32      ref5_colmv_use_flag     : 1;
        RK_U32      ref5_reserve            : 4;

        RK_U32      ref6_field              : 1;
        RK_U32      ref6_topfield_used      : 1;
        RK_U32      ref6_botfield_used      : 1;
        RK_U32      ref6_colmv_use_flag     : 1;
        RK_U32      ref6_reserve            : 4;

        RK_U32      ref7_field              : 1;
        RK_U32      ref7_topfield_used      : 1;
        RK_U32      ref7_botfield_used      : 1;
        RK_U32      ref7_colmv_use_flag     : 1;
        RK_U32      ref7_reserve            : 4;
    } ref4_7_info;

    struct SWREG101_H264_REG8_11_INFO {

        RK_U32      ref8_field              : 1;
        RK_U32      ref8_topfield_used      : 1;
        RK_U32      ref8_botfield_used      : 1;
        RK_U32      ref8_colmv_use_flag     : 1;
        RK_U32      ref8_reserve            : 4;

        RK_U32      ref9_field              : 1;
        RK_U32      ref9_topfield_used      : 1;
        RK_U32      ref9_botfield_used      : 1;
        RK_U32      ref9_colmv_use_flag     : 1;
        RK_U32      ref9_reserve            : 4;

        RK_U32      ref10_field             : 1;
        RK_U32      ref10_topfield_used     : 1;
        RK_U32      ref10_botfield_used     : 1;
        RK_U32      ref10_colmv_use_flag    : 1;
        RK_U32      ref10_reserve           : 4;

        RK_U32      ref11_field             : 1;
        RK_U32      ref11_topfield_used     : 1;
        RK_U32      ref11_botfield_used     : 1;
        RK_U32      ref11_colmv_use_flag    : 1;
        RK_U32      ref11_reserve           : 4;
    } ref8_11_info;

    struct SWREG102_H264_REG12_15_INFO {

        RK_U32      ref12_field             : 1;
        RK_U32      ref12_topfield_used     : 1;
        RK_U32      ref12_botfield_used     : 1;
        RK_U32      ref12_colmv_use_flag    : 1;
        RK_U32      ref12_reserve           : 4;

        RK_U32      ref13_field             : 1;
        RK_U32      ref13_topfield_used     : 1;
        RK_U32      ref13_botfield_used     : 1;
        RK_U32      ref13_colmv_use_flag    : 1;
        RK_U32      ref13_reserve           : 4;

        RK_U32      ref14_field             : 1;
        RK_U32      ref14_topfield_used     : 1;
        RK_U32      ref14_botfield_used     : 1;
        RK_U32      ref14_colmv_use_flag    : 1;
        RK_U32      ref14_reserve           : 4;

        RK_U32      ref15_field             : 1;
        RK_U32      ref15_topfield_used     : 1;
        RK_U32      ref15_botfield_used     : 1;
        RK_U32      ref15_colmv_use_flag    : 1;
        RK_U32      ref15_reserve           : 4;
    } ref12_15_info;

    struct SWREG103_111_NO_USE_REGS {
        RK_U32  reserve;
    } no_use_regs[9];

    struct SWREG112_ERROR_REF_INFO {
        RK_U32      avs2_ref_error_field        : 1;
        RK_U32      avs2_ref_error_topfield     : 1;
        RK_U32      ref_error_topfield_used     : 1;
        RK_U32      ref_error_botfield_used     : 1;
        RK_U32      reserve                     : 28;
    } err_ref_info;
} Vdpu34xRegH264dParam;

/* base: OFFSET_CODEC_ADDR_REGS */
typedef struct Vdpu34xRegH264dAddr_t {
    /* SWREG160 */
    RK_U32  reg160_no_use;

    /* SWREG161 */
    RK_U32  pps_base;

    /* SWREG162 */
    RK_U32  reg162_no_use;

    /* SWREG163 */
    RK_U32  rps_base;

    /* SWREG164~179 */
    RK_U32  ref_base[16];

    /* SWREG180 */
    RK_U32  scanlist_addr;

    /* SWREG181~196 */
    RK_U32  colmv_base[16];

    /* SWREG197 */
    RK_U32  cabactbl_base;
} Vdpu34xRegH264dAddr;

typedef struct Vdpu34xH264dRegSet_t {
    Vdpu34xRegCommon        common;
    Vdpu34xRegH264dParam    h264d_param;
    Vdpu34xRegCommonAddr    common_addr;
    Vdpu34xRegH264dAddr     h264d_addr;
    Vdpu34xRegIrqStatus     irq_status;
} Vdpu34xH264dRegSet;

#endif /* __VDPU34X_H264D_H__ */
