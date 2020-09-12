/*
 * Copyright 2016 Rockchip Electronics Co. LTD
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

#define MODULE_TAG "rc_model_v2"

#include <math.h>

#include "mpp_env.h"
#include "mpp_mem.h"
#include "mpp_common.h"
#include "rc_base.h"
#include "rc_debug.h"
#include "rc_model_v2.h"
#include "string.h"

#define I_WINDOW_LEN 2
#define P_WINDOW1_LEN 5
#define P_WINDOW2_LEN 8
#define UPSCALE 8000

static const RK_S32 max_i_delta_qp[51] = {
    640, 640, 640, 640, 640, 640, 640, 640, 640, 640, 640, 640, 640, 640,
    576, 576, 512, 512, 448, 448, 384, 384, 320, 320, 320, 256, 256, 256,
    192, 192, 192, 192, 192, 128, 128, 128, 128, 128, 128, 64,  64,  64,
    64,  64,  64,  0,   0,   0,   0,   0,   0,
};

RK_S32 tab_lnx[64] = {
    -1216, -972, -830, -729, -651, -587, -533, -486,
    -445, -408, -374, -344, -316, -290, -265, -243,
    -221, -201, -182, -164, -147, -131, -115, -100,
    -86,  -72,  -59,  -46,  -34,  -22,  -11,    0,
    10,   21,   31,   41,   50,   60,   69,   78,
    86,   95,   87,  103,  111,  119,  127,  134,
    142,  149,  156,  163,  170,  177,  183,  190,
    196,  202,  208,  214,  220,  226,  232,  237,
};

RK_S32 mean_qp2scale[16] = {
    14,  15,  16, 18, 20, 22, 25, 28,
    32,  36,  40, 44, 50, 56, 64, 72
};
static const RK_S8 max_ip_qp_dealt[8] = {
    7, 7, 7, 7, 6, 4, 3, 2
};
RK_U32 bit2percent[100] = {
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 95, 90, 85, 80, 75, 72, 69, 66, 64,
    62, 59, 58, 57, 56, 55, 54, 53, 52, 51,
    50, 49, 48, 47, 47, 46, 46, 45, 45, 44,
    44, 43, 43, 43, 42, 42, 42, 41, 41, 41,
    40, 40, 40, 39, 39, 39, 38, 38, 38, 37,
    37, 37, 37, 37, 37, 37, 36, 36, 36, 36,
    36, 36, 36, 35, 35, 35, 34, 34, 34, 34,
    34, 33, 33, 33, 33, 33, 33, 33, 33, 33,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
};

typedef struct RcModelV2Ctx_t {
    RcCfg           usr_cfg;
    EncRcTaskInfo   hal_cfg;

    RK_U32          frame_type;
    RK_U32          last_frame_type;
    RK_S64          gop_total_bits;
    RK_U32          bit_per_frame;
    RK_U32          first_frm_flg;
    RK_S64          avg_gbits;
    RK_S64          real_gbits;

    MppDataV2       *i_bit;
    RK_U32          i_sumbits;
    RK_U32          i_scale;

    MppDataV2       *idr_bit;
    RK_U32          idr_sumbits;
    RK_U32          idr_scale;

    MppDataV2       *vi_bit;
    RK_U32          vi_sumbits;
    RK_U32          vi_scale;
    MppDataV2       *p_bit;
    RK_U32          p_sumbits;
    RK_U32          p_scale;

    MppDataV2       *pre_p_bit;
    MppDataV2       *pre_i_bit;
    MppDataV2       *pre_i_mean_qp;
    MppDataV2       *madi;
    MppDataV2       *madp;

    RK_S32          target_bps;
    RK_S32          pre_target_bits;
    RK_S32          pre_real_bits;
    RK_S32          frm_bits_thr;
    RK_S32          ins_bps;
    RK_S32          last_inst_bps;

    RK_U32          motion_sensitivity;
    RK_U32          min_still_percent;
    RK_U32          max_still_qp;
    RK_S32          moving_ratio;
    RK_U32          pre_mean_qp;
    RK_U32          pre_i_scale;
    /*super frame thr*/
    RK_U32          super_ifrm_bits_thr;
    RK_U32          super_pfrm_bits_thr;
    MppDataV2       *stat_bits;
    MppDataV2       *gop_bits;
    MppDataV2       *stat_rate;
    RK_S32          watl_thrd;
    RK_S32          stat_watl;
    RK_S32          watl_base;

    RK_S32          next_i_ratio;      // scale 64
    RK_S32          next_ratio;        // scale 64
    RK_S32          pre_i_qp;
    RK_S32          pre_p_qp;
    RK_U32          scale_qp;          // scale 64
    MppDataV2       *means_qp;
    RK_U32          frm_num;

    /*qp decision*/
    RK_S32          cur_scale_qp;
    RK_S32          start_qp;
    RK_S32          prev_quality;
    RK_S32          prev_md_prop;

    RK_S32          reenc_cnt;
    RK_U32          drop_cnt;
    RK_S32          on_drop;
    RK_S32          on_pskip;
} RcModelV2Ctx;

MPP_RET bits_model_deinit(RcModelV2Ctx *ctx)
{
    rc_dbg_func("enter %p\n", ctx);

    if (ctx->i_bit != NULL) {
        mpp_data_deinit_v2(ctx->i_bit);
        ctx->i_bit = NULL;
    }

    if (ctx->p_bit != NULL) {
        mpp_data_deinit_v2(ctx->p_bit);
        ctx->p_bit = NULL;
    }

    if (ctx->vi_bit != NULL) {
        mpp_data_deinit_v2(ctx->vi_bit);
        ctx->vi_bit = NULL;
    }
    if (ctx->pre_p_bit != NULL) {
        mpp_data_deinit_v2(ctx->pre_p_bit);
        ctx->pre_p_bit = NULL;
    }

    if (ctx->pre_i_mean_qp != NULL) {
        mpp_data_deinit_v2(ctx->pre_i_mean_qp);
        ctx->pre_i_mean_qp = NULL;
    }

    if (ctx->madi != NULL) {
        mpp_data_deinit_v2(ctx->madi);
        ctx->madi = NULL;
    }
    if (ctx->madp != NULL) {
        mpp_data_deinit_v2(ctx->madp);
        ctx->madp = NULL;
    }

    if (ctx->stat_rate != NULL) {
        mpp_data_deinit_v2(ctx->stat_rate);
        ctx->stat_rate = NULL;
    }

    if (ctx->stat_bits != NULL) {
        mpp_data_deinit_v2(ctx->stat_bits);
        ctx->stat_bits = NULL;
    }
    if (ctx->gop_bits != NULL) {
        mpp_data_deinit_v2(ctx->gop_bits);
        ctx->gop_bits = NULL;
    }
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

void bits_frm_init(RcModelV2Ctx *ctx)
{
    rc_dbg_func("enter %p\n", ctx);
    RK_U32 gop_len = ctx->usr_cfg.igop;
    RK_U32 p_bit = 0;
    switch (ctx->usr_cfg.gop_mode) {
    case NORMAL_P: {
        ctx->i_scale = 480;
        ctx->p_scale = 16;
        if (gop_len <= 1)
            p_bit = ctx->gop_total_bits * 16;
        else
            p_bit = ctx->gop_total_bits * 16 / (ctx->i_scale + ctx->p_scale * (gop_len - 1));
        mpp_data_reset_v2(ctx->p_bit, p_bit);
        ctx->p_sumbits = 5 * p_bit;
        mpp_data_reset_v2(ctx->i_bit, p_bit * ctx->i_scale / 16);
        ctx->i_sumbits = 2 * p_bit * ctx->i_scale / 16;
    } break;
    case SMART_P: {
        RK_U32 vi_num = 0;
        mpp_assert(ctx->usr_cfg.vgop > 1);
        ctx->i_scale = 320;
        ctx->p_scale = 16;
        ctx->vi_scale = 32;
        vi_num = gop_len / ctx->usr_cfg.vgop;
        if (vi_num > 0) {
            vi_num = vi_num - 1;
        }
        p_bit = ctx->gop_total_bits * 16 / (ctx->i_scale + ctx->vi_scale * vi_num + ctx->p_scale * (gop_len - vi_num));
        mpp_data_reset_v2(ctx->p_bit, p_bit);
        ctx->p_sumbits = 5 * p_bit;

        mpp_data_reset_v2(ctx->i_bit, p_bit * ctx->i_scale / 16);
        ctx->i_sumbits = 2 * p_bit * ctx->i_scale / 16;

        mpp_data_reset_v2(ctx->vi_bit, p_bit * ctx->vi_scale / 16);
        ctx->vi_sumbits = 2 * p_bit * ctx->vi_scale / 16;
    } break;
    default:
        break;
    }
    rc_dbg_rc("p_sumbits %d i_sumbits %d vi_sumbits %d\n", ctx->p_sumbits, ctx->i_sumbits, ctx->vi_sumbits);
    rc_dbg_func("leave %p\n", ctx);
}

MPP_RET bits_model_init(RcModelV2Ctx *ctx)
{
    RK_S32 gop_len = ctx->usr_cfg.igop;
    RcFpsCfg *fps = &ctx->usr_cfg.fps;
    RK_S64 gop_bits = 0;
    RK_U32 stat_times = ctx->usr_cfg.stat_times;
    RK_U32 stat_len;
    RK_U32 target_bps;

    rc_dbg_func("enter %p\n", ctx);

    if (stat_times == 0) {
        stat_times = 3;
        ctx->usr_cfg.stat_times = stat_times;
    }

    if (ctx->usr_cfg.max_i_bit_prop <= 0) {
        ctx->usr_cfg.max_i_bit_prop = 30;
    } else if (ctx->usr_cfg.max_i_bit_prop > 100) {
        ctx->usr_cfg.max_i_bit_prop = 100;
    }
    rc_dbg_rc("max_i_bit_prop  %d",  ctx->usr_cfg.max_i_bit_prop);

    if (!gop_len || gop_len > 500) {
        mpp_log("infinte gop, set default for rc bit calc\n");
        ctx->usr_cfg.igop = gop_len = 500;
    }
    if (!ctx->min_still_percent) {
        if (ctx->usr_cfg.bps_min && ctx->usr_cfg.bps_max) {
            ctx->min_still_percent = ctx->usr_cfg.bps_min * 100 / ctx->usr_cfg.bps_max;
        } else {
            ctx->min_still_percent = 25;
        }
        rc_dbg_rc("min_still_percent  %d", ctx->min_still_percent);
    }
    ctx->max_still_qp = 35;
    ctx->super_ifrm_bits_thr = -1;
    ctx->super_pfrm_bits_thr = -1;
    ctx->motion_sensitivity = 90;

    ctx->first_frm_flg = 1;

    stat_len = fps->fps_out_num * ctx->usr_cfg.stat_times / fps->fps_out_denorm;
    if ( ctx->usr_cfg.mode == RC_FIXQP) {
        return MPP_OK;
    } else if (ctx->usr_cfg.mode == RC_CBR) {
        target_bps = ctx->usr_cfg.bps_target;
    } else {
        target_bps = ctx->usr_cfg.bps_max;
    }
    ctx->target_bps = ctx->usr_cfg.bps_target;

    if (gop_len >= 1)
        gop_bits = (RK_S64)gop_len * target_bps * fps->fps_out_denorm;
    else
        gop_bits = (RK_S64)fps->fps_out_num * target_bps * fps->fps_out_denorm;

    ctx->gop_total_bits = gop_bits / fps->fps_out_num;

    bits_model_deinit(ctx);
    mpp_data_init_v2(&ctx->i_bit, I_WINDOW_LEN);
    if (ctx->i_bit == NULL) {
        mpp_err("i_bit init fail");
        return -1;
    }

    mpp_data_init_v2(&ctx->vi_bit, I_WINDOW_LEN);
    if (ctx->vi_bit == NULL) {
        mpp_err("vi_bit init fail");
        return -1;
    }
    mpp_data_init_v2(&ctx->p_bit, P_WINDOW1_LEN);
    if (ctx->p_bit == NULL) {
        mpp_err("p_bit init fail");
        return -1;
    }

    mpp_data_init_v2(&ctx->pre_p_bit, P_WINDOW2_LEN);
    if (ctx->pre_p_bit == NULL) {
        mpp_err("pre_p_bit init fail");
        return -1;
    }
    mpp_data_init_v2(&ctx->pre_i_bit, I_WINDOW_LEN);
    if (ctx->pre_i_bit == NULL) {
        mpp_err("pre_i_bit init fail");
        return -1;
    }

    mpp_data_init_v2(&ctx->pre_i_mean_qp, I_WINDOW_LEN);
    if (ctx->pre_i_mean_qp == NULL) {
        mpp_err("pre_i_mean_qp init fail");
        return -1;
    }
    mpp_data_reset_v2(ctx->pre_i_mean_qp, -1);
    mpp_data_init_v2(&ctx->madi, P_WINDOW2_LEN);
    if (ctx->madi == NULL) {
        mpp_err("madi init fail");
        return -1;
    }

    mpp_data_init_v2(&ctx->madp, P_WINDOW2_LEN);
    if (ctx->madp == NULL) {
        mpp_err("madp init fail");
        return -1;
    }

    mpp_data_init_v2(&ctx->stat_rate, fps->fps_out_num);
    if (ctx->stat_rate == NULL) {
        mpp_err("stat_rate init fail fps_out_num %d", fps->fps_out_num);
        return -1;
    }

    mpp_data_init_v2(&ctx->stat_bits, stat_len);
    if (ctx->stat_bits == NULL) {
        mpp_err("stat_bits init fail stat_len %d", stat_len);
        return -1;
    }

    mpp_data_reset_v2(ctx->stat_rate, 0);

    mpp_data_init_v2(&ctx->gop_bits, gop_len);
    if (ctx->gop_bits == NULL) {
        mpp_err("gop_bits init fail gop_len %d", gop_len);
        return -1;
    }
    mpp_data_reset_v2(ctx->gop_bits, 0);

    ctx->bit_per_frame = target_bps * fps->fps_out_denorm / fps->fps_out_num;
    ctx->watl_thrd = 3 * target_bps;
    ctx->stat_watl = ctx->watl_thrd  >> 3;
    ctx->watl_base = ctx->stat_watl;

    mpp_data_reset_v2(ctx->stat_bits, ctx->bit_per_frame);
    rc_dbg_rc("gop %d total bit %lld per_frame %d statistics time %d second\n",
              ctx->usr_cfg.igop, ctx->gop_total_bits, ctx->bit_per_frame,
              ctx->usr_cfg.stat_times);

    bits_frm_init(ctx);
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

RK_S32 moving_judge_update(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    switch (ctx->frame_type) {
    case INTRA_FRAME: {
        mpp_data_update_v2(ctx->pre_i_bit, cfg->bit_real);
        mpp_data_update_v2(ctx->pre_i_mean_qp, cfg->quality_real);
    } break;

    case INTER_P_FRAME: {
        mpp_data_update_v2(ctx->pre_p_bit, cfg->bit_real);
        mpp_data_update_v2(ctx->madp, cfg->madp);
    } break;

    default:
        break;
    }
    return 0;
}

void bit_statics_update(RcModelV2Ctx *ctx, RK_U32 real_bit)
{
    RK_S32 mean_pbits, mean_ibits;
    RcCfg  *usr_cfg = &ctx->usr_cfg;
    RK_U32 gop_len = usr_cfg->igop;

    mpp_data_update_v2(ctx->gop_bits, real_bit);
    mean_pbits = mpp_data_mean_v2(ctx->pre_p_bit);
    mean_ibits = mpp_data_mean_v2(ctx->pre_i_bit);
    ctx->real_gbits  = mpp_data_sum_v2(ctx->gop_bits);
    ctx->avg_gbits  = (gop_len - 1) * (RK_S64)mean_pbits + mean_ibits;
}

MPP_RET bits_model_update(RcModelV2Ctx *ctx, RK_S32 real_bit, RK_U32 madi)
{
    RK_S32 water_level = 0;

    rc_dbg_func("enter %p\n", ctx);

    mpp_data_update_v2(ctx->stat_rate, real_bit != 0);
    mpp_data_update_v2(ctx->stat_bits, real_bit);
    ctx->ins_bps = mpp_data_sum_v2(ctx->stat_bits) / ctx->usr_cfg.stat_times;
    if (real_bit + ctx->stat_watl > ctx->watl_thrd)
        water_level = ctx->watl_thrd - ctx->bit_per_frame;
    else
        water_level = real_bit + ctx->stat_watl - ctx->bit_per_frame;

    if (water_level < 0) {
        water_level = 0;
    }
    ctx->stat_watl = water_level;
    switch (ctx->frame_type) {
    case INTRA_FRAME: {
        mpp_data_update_v2(ctx->i_bit, real_bit);
        ctx->i_sumbits = mpp_data_sum_v2(ctx->i_bit);
        ctx->i_scale = 80 * ctx->i_sumbits / (2 * ctx->p_sumbits);
        rc_dbg_rc("i_sumbits %d p_sumbits %d i_scale %d\n",
                  ctx->i_sumbits, ctx->p_sumbits, ctx->i_scale);
    } break;

    case INTER_P_FRAME: {
        mpp_data_update_v2(ctx->p_bit, real_bit);
        mpp_data_update_v2(ctx->madi,  madi);
        ctx->p_sumbits = mpp_data_sum_v2(ctx->p_bit);
        ctx->p_scale = 16;
    } break;

    case INTER_VI_FRAME: {
        mpp_data_update_v2(ctx->vi_bit, real_bit);
        ctx->vi_sumbits = mpp_data_sum_v2(ctx->vi_bit);
        ctx->vi_scale = 80 * ctx->vi_sumbits / (2 * ctx->p_sumbits);
    } break;

    default:
        break;
    }

    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET bits_model_alloc(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg, RK_S64 total_bits)
{
    RK_U32 max_i_prop = ctx->usr_cfg.max_i_bit_prop * 16;
    RK_S32 gop_len = ctx->usr_cfg.igop;
    RK_S32 i_scale = ctx->i_scale;
    RK_S32 vi_scale = ctx->vi_scale;
    RK_S32 alloc_bits = 0;

    ctx->i_scale = 80 * ctx->i_sumbits / (2 * ctx->p_sumbits);
    i_scale = ctx->i_scale;

    rc_dbg_func("enter %p\n", ctx);
    rc_dbg_rc("frame_type %d max_i_prop %d i_scale %d total_bits %lld\n",
              ctx->frame_type, max_i_prop, i_scale, total_bits);

    if (ctx->usr_cfg.gop_mode == SMART_P) {
        RK_U32 vi_num = 0;
        mpp_assert(ctx->usr_cfg.vgop > 1);
        vi_num = gop_len / ctx->usr_cfg.vgop;
        if (vi_num > 0) {
            vi_num = vi_num - 1;
        }
        switch (ctx->frame_type) {
        case INTRA_FRAME: {
            i_scale = mpp_clip(i_scale, 16, 16000);
            total_bits = total_bits * i_scale;
        } break;

        case INTER_P_FRAME: {
            i_scale = mpp_clip(i_scale, 16, max_i_prop);
            total_bits = total_bits * 16;
        } break;
        case INTER_VI_FRAME: {
            i_scale = mpp_clip(i_scale, 16, max_i_prop);
            total_bits = total_bits * vi_scale;
        } break;
        default:
            break;
        }
        alloc_bits = total_bits / (i_scale + 16 * (gop_len - vi_num) + vi_num * vi_scale);
    } else {
        switch (ctx->frame_type) {
        case INTRA_FRAME: {
            if (ctx->usr_cfg.mode == RC_CBR)
                i_scale = mpp_clip(i_scale, 16, 800);
            else
                i_scale = mpp_clip(i_scale, 16, 16000);

            total_bits = total_bits * i_scale;
        } break;

        case INTER_P_FRAME: {
            i_scale = mpp_clip(i_scale, 16, max_i_prop);
            total_bits = total_bits * 16;
        } break;
        default:
            break;
        }
        if (gop_len > 1) {
            alloc_bits = total_bits / (i_scale + 16 * (gop_len - 1));
        } else {
            alloc_bits = total_bits / i_scale;
        }
    }
    rc_dbg_rc("i_scale  %d, total_bits %lld", i_scale, total_bits);
    cfg->bit_target = alloc_bits;

    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET calc_next_i_ratio(RcModelV2Ctx *ctx)
{
    RK_S32 max_i_prop = ctx->usr_cfg.max_i_bit_prop * 16;
    RK_S32 gop_len    = ctx->usr_cfg.igop;
    RK_S32 pre_qp     = ctx->pre_i_qp;
    RK_S32 bits_alloc;

    rc_dbg_func("enter %p\n", ctx);
    if (gop_len > 1) {
        bits_alloc = ctx->gop_total_bits * max_i_prop / (max_i_prop + 16 * (gop_len - 1));
    } else {
        bits_alloc = ctx->gop_total_bits * max_i_prop / max_i_prop;
    }

    if (ctx->pre_real_bits > bits_alloc || ctx->next_i_ratio) {
        RK_S32 ratio = ((ctx->pre_real_bits - bits_alloc) << 8) / bits_alloc;

        ratio = mpp_clip(ratio, -256, 256);
        ratio = ctx->next_i_ratio + ratio;
        if (ratio >= 0) {
            if (ratio > max_i_delta_qp[pre_qp])
                ratio = max_i_delta_qp[pre_qp];
        } else {
            ratio = 0;
        }
        ctx->next_i_ratio = ratio;
        rc_dbg_rc("ctx->next_i_ratio %d", ctx->next_i_ratio);
    }

    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;

}

MPP_RET calc_cbr_ratio(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    RK_S32 target_bps = ctx->target_bps;
    RK_S32 ins_bps = ctx->ins_bps;
    RK_S32 pre_target_bits = ctx->pre_target_bits;
    RK_S32 pre_real_bits = ctx->pre_real_bits;
    RK_S32 pre_ins_bps = ctx->last_inst_bps;
    RK_S32 idx1, idx2;
    RK_S32 bit_diff_ratio, ins_ratio, bps_ratio, wl_ratio;
    RK_S32 flag = 0;
    RK_S32 fluc_l = 3;

    rc_dbg_func("enter %p\n", ctx);

    rc_dbg_bps("%10s|%10s|%10s|%10s|%10s|%8s", "r_bits", "t_bits", "ins_bps", "p_ins_bps", "target_bps", "watl");
    rc_dbg_bps("%10d %10d %10d %10d %10d %8d", pre_real_bits, pre_target_bits, ins_bps, pre_ins_bps, target_bps, ctx->stat_watl >> 10);

    bits_model_alloc(ctx, cfg, ctx->gop_total_bits);

    mpp_assert(target_bps > 0);


    if (pre_target_bits > pre_real_bits)
        bit_diff_ratio = 52 * (pre_real_bits - pre_target_bits) / pre_target_bits;
    else
        bit_diff_ratio = 64 * (pre_real_bits - pre_target_bits) / pre_target_bits;

    idx1 = (ins_bps << 5) / target_bps;
    idx2 = (pre_ins_bps << 5) / target_bps;

    idx1 = mpp_clip(idx1, 0, 64);
    idx2 = mpp_clip(idx2, 0, 64);
    ins_ratio = tab_lnx[idx1] - tab_lnx[idx2]; // %3


    /*ins_bps is increase and pre_ins > target_bps*15/16 will raise up ins_ratio to decrease bit
     *ins_bps is decresase and pre_ins < target_bps*17/16 will decrease ins_ratio to increase bit
     */

    if (ins_bps > pre_ins_bps && target_bps - pre_ins_bps < (target_bps >> 4)) { // %6
        ins_ratio = 6 * ins_ratio;
    } else if ( ins_bps < pre_ins_bps && pre_ins_bps - target_bps < (target_bps >> 4)) {
        ins_ratio = 4 * ins_ratio;
    } else {
        if (bit_diff_ratio < -128) {
            ins_ratio = -128;
            flag = 1;
        } else {
            ins_ratio = 0;
        }
    }

    bit_diff_ratio = mpp_clip(bit_diff_ratio, -128, 256);

    if (!flag) {
        ins_ratio = mpp_clip(ins_ratio, -128, 256);
        ins_ratio = bit_diff_ratio + ins_ratio;
    }

    bps_ratio = (ins_bps - target_bps) * fluc_l / (target_bps >> 4);
    wl_ratio = 4 * (ctx->stat_watl - ctx->watl_base) * fluc_l / ctx->watl_base;
    bps_ratio = mpp_clip(bps_ratio, -32, 32);
    wl_ratio  = mpp_clip(wl_ratio, -16, 32);
    ctx->next_ratio = ins_ratio + bps_ratio + wl_ratio;

    rc_dbg_qp("%10s|%10s|%10s|%10s|%10s|%10s", "diff_ratio", "ins_ratio", "bps_ratio",
              "wl_ratio", "next_ratio", "cur_qp_s");
    rc_dbg_qp("%10d %10d %10d %10d %10d|%10d", bit_diff_ratio, ins_ratio - bit_diff_ratio,
              bps_ratio, wl_ratio, ctx->next_ratio, ctx->cur_scale_qp);

    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET reenc_calc_cbr_ratio(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    RK_S32 stat_time = ctx->usr_cfg.stat_times;
    RK_S32 pre_ins_bps = mpp_data_sum_v2(ctx->stat_bits) / stat_time;
    RK_S32 ins_bps = (pre_ins_bps * stat_time - ctx->stat_bits->val[0] + cfg->bit_real) / stat_time;
    RK_S32 real_bit = cfg->bit_real;
    RK_S32 target_bit = cfg->bit_target;
    RK_S32 target_bps = ctx->target_bps;
    RK_S32 water_level = 0;
    RK_S32 idx1, idx2;
    RK_S32 bit_diff_ratio, ins_ratio, bps_ratio, wl_ratio;
    RK_S32 mb_w = MPP_ALIGN(ctx->usr_cfg.width, 16) / 16;
    RK_S32 mb_h = MPP_ALIGN(ctx->usr_cfg.height, 16) / 16;

    rc_dbg_func("enter %p\n", ctx);

    if (real_bit + ctx->stat_watl > ctx->watl_thrd)
        water_level = ctx->watl_thrd - ctx->bit_per_frame;
    else
        water_level = real_bit + ctx->stat_watl - ctx->bit_per_frame;

    if (water_level < 0) {
        water_level = 0;
    }

    if (target_bit > real_bit)
        bit_diff_ratio = 32 * (real_bit - target_bit) / target_bit;
    else
        bit_diff_ratio = 48 * (real_bit - target_bit) / real_bit;

    idx1 = ins_bps / (target_bps >> 5);
    idx2 = pre_ins_bps / (target_bps >> 5);

    idx1 = mpp_clip(idx1, 0, 64);
    idx2 = mpp_clip(idx2, 0, 64);
    ins_ratio = tab_lnx[idx1] - tab_lnx[idx2];

    bps_ratio = 96 * (ins_bps - target_bps) / target_bps;
    wl_ratio = 32 * (water_level - ctx->watl_base) /  ctx->watl_base;
    if (pre_ins_bps < ins_bps && target_bps != pre_ins_bps) {
        ins_ratio = 6 * ins_ratio;
        ins_ratio = mpp_clip(ins_ratio, -192, 256);
    } else {
        if (ctx->frame_type == INTRA_FRAME) {
            ins_ratio = 3 * ins_ratio;
            ins_ratio = mpp_clip(ins_ratio, -192, 256);
        } else {
            ins_ratio = 0;
        }
    }

    bit_diff_ratio = mpp_clip(bit_diff_ratio, -128, 256);
    bps_ratio = mpp_clip(bps_ratio, -32, 32);
    wl_ratio  = mpp_clip(wl_ratio, -32, 32);
    ctx->next_ratio = bit_diff_ratio + ins_ratio + bps_ratio + wl_ratio;
    if (ctx->frame_type  == INTRA_FRAME && (cfg->madi > 0)) {
        RK_U32 tar_bpp = target_bit / (mb_w * mb_h);
        float lnb_t = log(tar_bpp);
        float c = 6.7204, a = -0.1435, b = 0.0438;
        float start_qp = (ctx->cur_scale_qp >> 6);
        int qp_c = ((lnb_t - cfg->madi * b - c) / a + 14);
        if (qp_c > start_qp)
            ctx->next_ratio  = (qp_c << 6) - ctx->cur_scale_qp;
    }
    rc_dbg_rc("cbr target_bit %d real_bit %d reenc next ratio %d", target_bit, real_bit, ctx->next_ratio);
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}


MPP_RET calc_vbr_ratio(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    RK_S32 bps_change = ctx->target_bps;
    RK_S32 max_bps_target = ctx->usr_cfg.bps_max;
    RK_S32 ins_bps = ctx->ins_bps;
    RK_S32 pre_target_bits = ctx->pre_target_bits;
    RK_S32 pre_real_bits = ctx->pre_real_bits;
    RK_S32 pre_ins_bps = ctx->last_inst_bps;
    RK_S32 idx1, idx2;
    RK_S32 bit_diff_ratio, ins_ratio, bps_ratio;
    RK_S32 flag = 0;

    rc_dbg_func("enter %p\n", ctx);

    bits_model_alloc(ctx, cfg, ctx->gop_total_bits);
    if (pre_target_bits > pre_real_bits)
        bit_diff_ratio = 32 * (pre_real_bits - pre_target_bits) / pre_target_bits;
    else
        bit_diff_ratio = 64 * (pre_real_bits - pre_target_bits) / pre_target_bits;

    idx1 = ins_bps / (max_bps_target >> 5);
    idx2 = pre_ins_bps / (max_bps_target >> 5);

    idx1 = mpp_clip(idx1, 0, 64);
    idx2 = mpp_clip(idx2, 0, 64);
    ins_ratio = tab_lnx[idx1] - tab_lnx[idx2];

    rc_dbg_bps("%10s|%10s|%10s|%10s|%10s|%10s", "r_bits", "t_bits", "ins_bps", "p_ins_bps",
               "bps_ch", "max_bps");
    rc_dbg_bps("%10d %10d %10d %10d %10d %10d", pre_real_bits, pre_target_bits, ins_bps,
               pre_ins_bps, bps_change, max_bps_target);

    if (ins_bps <= bps_change || (ins_bps > bps_change && ins_bps <= pre_ins_bps)) {
        flag = ins_bps < pre_ins_bps;
        if (bps_change <= pre_ins_bps)
            flag = 0;
        if (!flag) {
            bit_diff_ratio = mpp_clip(bit_diff_ratio, -128, 256);
        } else {
            ins_ratio = 3 * ins_ratio;
        }
    } else {
        ins_ratio = 6 * ins_ratio;
    }
    ins_ratio = mpp_clip(ins_ratio, -128, 256);
    bps_ratio = 3 * (ins_bps - bps_change) / (max_bps_target >> 4);
    bps_ratio = mpp_clip(bps_ratio, -16, 32);
    if (ctx->i_scale > 640) {
        bit_diff_ratio = mpp_clip(bit_diff_ratio, -16, 32);
        ins_ratio = mpp_clip(ins_ratio, -16, 32);
    }

    ctx->next_ratio = bit_diff_ratio + ins_ratio + bps_ratio;

    rc_dbg_qp("%10s|%10s|%10s|%10s|%10s", "diff_ratio", "ins_ratio", "bps_ratio", "next_ratio", "cur_qp_s");
    rc_dbg_qp("%10d %10d %10d %10d|%10d", bit_diff_ratio, ins_ratio, bps_ratio, ctx->next_ratio, ctx->cur_scale_qp);
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET reenc_calc_vbr_ratio(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    RK_S32 stat_time = ctx->usr_cfg.stat_times;
    RK_S32 pre_ins_bps = mpp_data_sum_v2(ctx->stat_bits) / stat_time;
    RK_S32 ins_bps = (pre_ins_bps * stat_time - ctx->stat_bits->val[0] + cfg->bit_real) / stat_time;
    RK_S32 bps_change = ctx->target_bps;
    RK_S32 max_bps_target = ctx->usr_cfg.bps_max;
    RK_S32 real_bit = cfg->bit_real;
    RK_S32 target_bit = cfg->bit_target;
    RK_S32 idx1, idx2;
    RK_S32 bit_diff_ratio, ins_ratio, bps_ratio;

    rc_dbg_func("enter %p\n", ctx);

    if (target_bit <= real_bit)
        bit_diff_ratio = 32 * (real_bit - target_bit) / target_bit;
    else
        bit_diff_ratio = 32 * (real_bit - target_bit) / real_bit;

    idx1 = ins_bps / (max_bps_target >> 5);
    idx2 = pre_ins_bps / (max_bps_target >> 5);
    idx1 = mpp_clip(idx1, 0, 64);
    idx2 = mpp_clip(idx2, 0, 64);
    if (pre_ins_bps < ins_bps && bps_change < ins_bps) {
        ins_ratio = 6 * (tab_lnx[idx1] - tab_lnx[idx2]);
        ins_ratio = mpp_clip(ins_ratio, -192, 256);
    } else {
        ins_ratio = 0;
    }

    bps_ratio = 96 * (ins_bps - bps_change) / bps_change;
    bit_diff_ratio = mpp_clip(bit_diff_ratio, -128, 256);
    bps_ratio = mpp_clip(bps_ratio, -32, 32);

    ctx->next_ratio = bit_diff_ratio + ins_ratio + bps_ratio;
    rc_dbg_rc("vbr reenc next ratio %d", ctx->next_ratio);
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

RK_S32 moving_ratio_calc(RcModelV2Ctx *ctx)
{
    RK_S32 motion_sensitivity = ctx->motion_sensitivity;
    RK_S32 scale = 0, i;
    RK_S32 total_bit = 0, pbit_sum = 0;
    RK_S32 madi_sum = 0, madp_sum = 0;
    RK_S32 percent = 0;

    for (i = 0; i < 2; i++) {
        RK_S32 pre_I_bit = ctx->pre_i_bit->val[i];
        RK_S32 pre_mean_qp = ctx->pre_i_mean_qp->val[i];
        if (pre_mean_qp == -1) {
            scale = 32;
        } else {
            RK_S32 index = pre_mean_qp + 8 - ctx->pre_mean_qp;
            if (index >= 0) {
                index = mpp_clip(index, 0 , 15);
                scale = mean_qp2scale[index];
            } else {
                scale = 14;
            }
        }
        total_bit += (scale * pre_I_bit >> 5);
        rc_dbg_rc("pre_mean_qp = %d, ctx->pre_mean_qp %d", pre_mean_qp, ctx->pre_mean_qp);
        rc_dbg_rc("scale = %d, pre_I_bit %d", scale, pre_I_bit);
    }

    pbit_sum = mpp_data_sum_v2(ctx->pre_p_bit);
    madi_sum = mpp_data_sum_v2(ctx->madi);
    madp_sum = mpp_data_sum_v2(ctx->madp);
    rc_dbg_rc("pbit_sum %d,madi_sum = %d, madp_sum = %d", pbit_sum, madi_sum, madp_sum);
    if ( pbit_sum == 0 || total_bit == 0) {
        percent = 255;
    } else {
        RK_S32 index = (total_bit << 6) / pbit_sum;
        index = mpp_clip(index >> 4, 1, 99);
        percent = (bit2percent[index] << 8) / 100;
    }
    rc_dbg_rc("means qp percent %d min_still_percent %d", percent, ctx->min_still_percent);
    RK_S32 percent_a = (ctx->min_still_percent - 30) << 8;
    RK_S32 percent_b = 100 - ctx->min_still_percent;

    percent = (percent_a + percent * percent_b) / 70;
    rc_dbg_rc("percent_a = %d percent_b %d", percent_a, percent_b);
    RK_S32 mv_percnt = (ctx->prev_md_prop * 100) >> 8;
    RK_S32 mv_ratio = (percent_a + 7680 + mv_percnt * percent_b) / 100;
    rc_dbg_rc("mv_ratio = %d", mv_ratio);
    RK_S32 mad_ratio = 256;
    if (madi_sum) {
        mad_ratio = 20 * madp_sum / madi_sum;
        mad_ratio = mpp_clip(mad_ratio, 5, 100);
        rc_dbg_rc("mad_ratio = %d", mad_ratio);
        mad_ratio = (mad_ratio << 8) / 100;
    }
    mad_ratio = (percent_a + 7680 + percent_b * mad_ratio) / 100;

    RK_S32 moving_ratio = (percent + 1 + (mv_ratio * motion_sensitivity + (100 - motion_sensitivity) * mad_ratio) / 100) >> 1;
    rc_dbg_rc("moving_ratio = %d, motion_sensitivity = %d", moving_ratio, motion_sensitivity);
    rc_dbg_rc("percent %d mad_ratio %d hr_ratio %d, moving_ratio %d", percent, mad_ratio, mv_ratio, moving_ratio);
    return moving_ratio;
}


MPP_RET calc_avbr_ratio(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    RK_S32 bps_change = ctx->target_bps;
    RK_S32 max_bps_target = ctx->usr_cfg.bps_max;
    RK_S32 ins_bps = ctx->ins_bps;
    RK_S32 pre_target_bits = ctx->pre_target_bits;
    RK_S32 pre_real_bits = ctx->pre_real_bits;
    RK_S32 pre_ins_bps = ctx->last_inst_bps;
    RK_S32 idx1, idx2;
    RK_S32 bit_diff_ratio, ins_ratio, agop_dratio, rgop_dratio;
    RK_S32 moving_ratio = 0, moving_percent;
    RK_S32 gop_bits = 0, gop_kbits = 0;
    RK_S32 i_ratio, max_bps;
    RK_S32 qratio, final_qratio;
    rc_dbg_func("enter %p\n", ctx);
    moving_ratio = moving_ratio_calc(ctx);
    if (ctx->moving_ratio - 2 >= moving_ratio) {
        moving_ratio = ctx->moving_ratio - 2;
    }

    if (ctx->moving_ratio > moving_ratio && (ctx->max_still_qp << 6) <= ctx->scale_qp) {
        ctx->moving_ratio = mpp_clip(ctx->moving_ratio + 1, 0, 255);
    } else {
        ctx->moving_ratio = moving_ratio;
    }
    rc_dbg_rc("final moving_ratio = %d", moving_ratio);
    gop_bits = moving_ratio * ctx->gop_total_bits >> 8;
    gop_kbits = gop_bits >> 10;
    if (gop_kbits < 1) {
        gop_kbits = 1;
    }

    bits_model_alloc(ctx, cfg, gop_bits);
    bps_change = moving_ratio * bps_change >> 8;
    if (moving_ratio < 0) {
        moving_ratio += 255;
    }
    moving_percent = 100 * moving_ratio >> 8;

    rc_dbg_bps("%10s|%10s|%10s|%10s|%10s", "m_ratio", "r_bits", "t_bits", "ins_bps", "p_ins_bps");
    rc_dbg_bps("%10d %10d %10d %10d %10d", ctx->moving_ratio, pre_real_bits, pre_target_bits, ins_bps, pre_ins_bps);

    if (pre_target_bits > pre_real_bits)
        bit_diff_ratio = 32 * (pre_real_bits - pre_target_bits) / pre_target_bits;
    else
        bit_diff_ratio = 64 * (pre_real_bits - pre_target_bits) / pre_target_bits * moving_percent;

    i_ratio = mpp_clip(ctx->pre_i_scale >> 4, 10, 200);
    idx1 = ins_bps / (max_bps_target >> 5);
    idx2 = pre_ins_bps / (max_bps_target >> 5);
    idx1 = mpp_clip(idx1, 0, 64);
    idx2 = mpp_clip(idx2, 0, 64);
    ins_ratio = tab_lnx[idx2] - tab_lnx[idx1];
    max_bps = bps_change;
    if (max_bps < pre_ins_bps) {
        max_bps = pre_ins_bps;
    }
    if (ins_bps <= max_bps) {
        if (ins_bps < pre_ins_bps && bps_change > pre_ins_bps) {
            ins_ratio = 3 * ins_ratio;
        } else {
            ins_ratio = 0;
        }
    } else {
        ins_ratio = 6 * ins_ratio;
    }

    ins_ratio = mpp_clip(ins_ratio >> 2, -128, 256);
    bit_diff_ratio = mpp_clip(10 * bit_diff_ratio / i_ratio, -128, 256);
    rgop_dratio  = mpp_clip(24 * ((ctx->real_gbits - gop_bits) >> 10) / gop_kbits, -1, 1);
    agop_dratio  = mpp_clip(48 * ((ctx->avg_gbits - gop_bits) >> 10) / gop_kbits , -1, 1);
    if (ctx->pre_i_scale > 640) {
        bit_diff_ratio = mpp_clip(bit_diff_ratio, -16, 32);
        ins_ratio = mpp_clip(ins_ratio, -16, 32);
    }
    qratio = 0;
    final_qratio = ins_ratio + bit_diff_ratio + agop_dratio + rgop_dratio;
    if (max_bps_target >= ins_bps) {
        if (final_qratio > 0) {
            if (ctx->scale_qp >= (ctx->max_still_qp << 6)) {
                final_qratio = ins_ratio + agop_dratio + rgop_dratio;
                qratio = -6;
            }
        }
    }
    ctx->next_ratio = qratio + final_qratio;

    rc_dbg_qp("%10s|%10s|%10s|%10s|%10s|%10s|%10s", "diff_ratio", "ins_ratio", "rg_ratio",
              "ag_ratio", "qratio", "next_ratio", "cur_qp_s");
    rc_dbg_qp("%10d %10d %10d %10d %10d %10d %10d", bit_diff_ratio, ins_ratio, rgop_dratio,
              agop_dratio, qratio, ctx->next_ratio, ctx->cur_scale_qp);
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET bits_mode_reset(RcModelV2Ctx *ctx)
{
    rc_dbg_func("enter %p\n", ctx);
    (void) ctx;
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET check_super_frame(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    MPP_RET ret = MPP_OK;
    RK_S32 frame_type = ctx->frame_type;
    RK_U32 bits_thr = 0;
    if (frame_type == INTRA_FRAME) {
        bits_thr = ctx->super_ifrm_bits_thr;
    } else {
        bits_thr = ctx->super_pfrm_bits_thr;
    }
    if ((RK_U32)cfg->bit_real >= bits_thr) {
        ret = MPP_NOK;
    }
    return ret;
}

MPP_RET check_re_enc(RcModelV2Ctx *ctx, EncRcTaskInfo *cfg)
{
    RcCfg *usr_cfg = &ctx->usr_cfg;
    RK_S32 frame_type = ctx->frame_type;
    RK_S32 bit_thr = 0;
    RK_S32 stat_time = ctx->usr_cfg.stat_times;
    RK_S32 last_ins_bps = mpp_data_sum_v2(ctx->stat_bits) / stat_time;
    RK_S32 ins_bps = (last_ins_bps * stat_time - ctx->stat_bits->val[ctx->stat_bits->size - 1]
                      + cfg->bit_real) / stat_time;
    RK_S32 target_bps;
    RK_S32 ret = MPP_OK;

    rc_dbg_func("enter %p\n", ctx);
    rc_dbg_rc("reenc check target_bps %d last_ins_bps %d ins_bps %d", ctx->usr_cfg.bps_target, last_ins_bps, ins_bps);

    if (ctx->reenc_cnt >= ctx->usr_cfg.max_reencode_times)
        return MPP_OK;

    rc_dbg_drop("drop mode %d frame_type %d\n", usr_cfg->drop_mode, frame_type);

    if (usr_cfg->drop_mode && frame_type == INTER_P_FRAME) {
        bit_thr = (RK_S32)(ctx->usr_cfg.bps_max * (100 + usr_cfg->drop_thd) / (float)100);
        rc_dbg_drop("drop mode %d check max_bps %d bit_thr %d ins_bps %d",
                    usr_cfg->drop_mode, ctx->usr_cfg.bps_target, bit_thr, ins_bps);
        return (ins_bps > bit_thr) ? MPP_NOK : MPP_OK;
    }

    switch (frame_type) {
    case INTRA_FRAME:
        bit_thr = 3 * cfg->bit_target / 2;
        break;
    case INTER_P_FRAME:
        bit_thr = 3 * cfg->bit_target;
        break;
    default:
        break;
    }

    if (cfg->bit_real > bit_thr) {
        if (ctx->usr_cfg.mode == RC_CBR) {
            target_bps = ctx->usr_cfg.bps_target;
            if (target_bps / 20 < ins_bps - last_ins_bps &&
                (target_bps + target_bps / 10 < ins_bps
                 || target_bps - target_bps / 10 > ins_bps)) {
                ret =  MPP_NOK;
            }
        } else {
            target_bps = ctx->usr_cfg.bps_max;
            if ((target_bps - (target_bps >> 3) < ins_bps) &&
                (target_bps / 20  < ins_bps - last_ins_bps)) {
                ret =  MPP_NOK;
            }
        }
    }

    rc_dbg_func("leave %p ret %d\n", ctx, ret);
    return ret;
}


MPP_RET rc_model_v2_init(void *ctx, RcCfg *cfg)
{
    RcModelV2Ctx *p = (RcModelV2Ctx*)ctx;

    rc_dbg_func("enter %p\n", ctx);

    memcpy(&p->usr_cfg, cfg, sizeof(RcCfg));
    bits_model_init(p);

    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET rc_model_v2_deinit(void *ctx)
{
    RcModelV2Ctx *p = (RcModelV2Ctx *)ctx;

    rc_dbg_func("enter %p\n", ctx);
    bits_model_deinit(p);

    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET rc_model_v2_start(void *ctx, EncRcTask *task)
{
    RcModelV2Ctx *p = (RcModelV2Ctx*)ctx;
    EncFrmStatus *frm = &task->frm;
    EncRcTaskInfo *info = &task->info;
    RcCfg *cfg = &p->usr_cfg;

    rc_dbg_func("enter %p\n", ctx);

    if (cfg->mode == RC_FIXQP) {
        if (cfg->init_quality <= 0) {
            mpp_log("invalid fix %d qp found set default qp 26\n",
                    cfg->init_quality);
            cfg->init_quality = 26;
        }

        if (cfg->max_quality <= 0)
            cfg->max_quality = cfg->init_quality;
        if (cfg->min_quality <= 0)
            cfg->min_quality = cfg->init_quality;
        if (cfg->max_i_quality <= 0)
            cfg->max_i_quality = cfg->max_quality;
        if (cfg->min_i_quality <= 0)
            cfg->min_i_quality = cfg->min_quality;

        if (frm->is_intra) {
            info->quality_max = cfg->max_i_quality;
            info->quality_min = cfg->min_i_quality;
            info->quality_target = cfg->min_i_quality;
        } else {
            info->quality_max = cfg->max_quality;
            info->quality_min = cfg->min_quality;
            info->quality_target = cfg->min_quality;
        }

        rc_dbg_rc("seq_idx %d intra %d\n", frm->seq_idx, frm->is_intra);
        rc_dbg_rc("bitrate [%d : %d : %d]\n", info->bit_min, info->bit_target, info->bit_max);
        rc_dbg_rc("quality [%d : %d : %d]\n", info->quality_min, info->quality_target, info->quality_max);

        return MPP_OK;
    }

    p->frame_type = (frm->is_intra) ? (INTRA_FRAME) : (INTER_P_FRAME);

    if (frm->ref_mode == REF_TO_PREV_INTRA) {
        p->frame_type = INTER_VI_FRAME;
    }

    p->next_ratio = 0;
    if (p->last_frame_type == INTRA_FRAME) {
        calc_next_i_ratio(p);
    }

    if (!p->first_frm_flg) {
        switch (p->usr_cfg.mode) {
        case RC_CBR: {
            calc_cbr_ratio(p, info);
        } break;
        case RC_VBR: {
            calc_vbr_ratio(p, info);
        } break;
        case RC_AVBR: {
            calc_avbr_ratio(p, info);
        } break;
        default:
            mpp_log("rc mode set error");
            break;
        }
    } else {
        bits_model_alloc(p, info, p->gop_total_bits);
    }

    /* quality determination */
    if (p->first_frm_flg)
        info->quality_target = -1;
    if (frm->is_intra) {
        info->quality_max = cfg->max_i_quality;
        info->quality_min = cfg->min_i_quality;
    } else {
        info->quality_max = cfg->max_quality;
        info->quality_min = cfg->min_quality;
    }

    rc_dbg_rc("seq_idx %d intra %d\n", frm->seq_idx, frm->is_intra);
    rc_dbg_rc("bitrate [%d : %d : %d]\n", info->bit_min, info->bit_target, info->bit_max);
    rc_dbg_rc("quality [%d : %d : %d]\n", info->quality_min, info->quality_target, info->quality_max);

    p->reenc_cnt = 0;

    rc_dbg_func("leave %p\n", ctx);

    return MPP_OK;
}

static RK_U32 mb_num[9] = {
    0,      200,    700,    1200,
    2000,   4000,   8000,   16000,
    20000
};

static RK_U32 tab_bit[9] = {
    3780,  3570,  3150,  2940,
    2730,  3780,  2100,  1680,
    2100
};

static RK_U8 qscale2qp[96] = {
    15,  15,  15,  15,  15,  16, 18, 20, 21, 22, 23,
    24,  25,  25,  26,  27,  28, 28, 29, 29, 30, 30,
    30,  31,  31,  32,  32,  33, 33, 33, 34, 34, 34,
    34,  35,  35,  35,  36,  36, 36, 36, 36, 37, 37,
    37,  37,  38,  38,  38,  38, 38, 39, 39, 39, 39,
    39,  39,  40,  40,  40,  40, 41, 41, 41, 41, 41,
    41,  41,  42,  42,  42,  42, 42, 42, 42, 42, 43,
    43,  43,  43,  43,  43,  43, 43, 44, 44, 44, 44,
    44,  44,  44,  44,  45,  45, 45, 45,
};

static RK_S32 cal_first_i_start_qp(RK_S32 target_bit, RK_U32 total_mb)
{
    RK_S32 cnt = 0;
    RK_S32 index;
    RK_S32 i;

    for (i = 0; i < 8; i++) {
        if (mb_num[i] > total_mb)
            break;
        cnt++;
    }

    index = (total_mb * tab_bit[cnt] - 350) / target_bit; // qscale
    index = mpp_clip(index, 4, 95);

    return qscale2qp[index];
}

MPP_RET rc_model_v2_hal_start(void *ctx, EncRcTask *task)
{
    RcModelV2Ctx *p = (RcModelV2Ctx *)ctx;
    EncFrmStatus *frm = &task->frm;
    EncRcTaskInfo *info = &task->info;
    EncRcForceCfg *force = &task->force;
    RK_S32 mb_w = MPP_ALIGN(p->usr_cfg.width, 16) / 16;
    RK_S32 mb_h = MPP_ALIGN(p->usr_cfg.height, 16) / 16;
    RK_S32 bit_min = info->bit_min;
    RK_S32 bit_max = info->bit_max;
    RK_S32 bit_target = info->bit_target;
    RK_S32 quality_min = info->quality_min;
    RK_S32 quality_max = info->quality_max;
    RK_S32 quality_target = info->quality_target;

    rc_dbg_func("enter p %p task %p\n", p, task);

    rc_dbg_rc("seq_idx %d intra %d\n", frm->seq_idx, frm->is_intra);

    if (force->force_flag & ENC_RC_FORCE_QP) {
        RK_S32 qp = force->force_qp;
        info->quality_target = qp;
        info->quality_max = qp;
        info->quality_min = qp;
        return MPP_OK;
    }

    if (p->usr_cfg.mode == RC_FIXQP)
        return MPP_OK;

    /* setup quality parameters */
    if (p->first_frm_flg && frm->is_intra) {
        if (info->quality_target < 0) {
            if (info->bit_target) {
                p->start_qp = cal_first_i_start_qp(info->bit_target, mb_w * mb_h);
                p->cur_scale_qp = (p->start_qp) << 6;
            } else {
                mpp_log("fix qp case but init qp no set");
                info->quality_target = 26;
                p->start_qp = 26;
                p->cur_scale_qp = (p->start_qp) << 6;
            }
        } else {
            p->start_qp = info->quality_target;
            p->cur_scale_qp = (p->start_qp) << 6;
        }

        if (p->reenc_cnt > 0) {
            p->cur_scale_qp += p->next_ratio;
            p->start_qp = p->cur_scale_qp >> 6;
            rc_dbg_rc("p->start_qp = %d, p->cur_scale_qp %d,p->next_ratio %d ", p->start_qp, p->cur_scale_qp, p->next_ratio);
        } else {
            p->start_qp -= p->usr_cfg.i_quality_delta;
        }
        p->cur_scale_qp = mpp_clip(p->cur_scale_qp, (info->quality_min << 6), (info->quality_max << 6));
        p->pre_i_qp = p->cur_scale_qp >> 6;
        p->pre_p_qp = p->cur_scale_qp >> 6;
    } else {
        RK_S32 qp_scale = p->cur_scale_qp + p->next_ratio;
        RK_S32 start_qp = 0;
        RK_S32 dealt_qp = 0;
        if (frm->is_intra) {
            qp_scale = mpp_clip(qp_scale, (info->quality_min << 6), (info->quality_max << 6));

            start_qp = ((p->pre_i_qp + ((qp_scale + p->next_i_ratio) >> 6)) >> 1);

            start_qp = mpp_clip(start_qp, info->quality_min, info->quality_max);
            p->pre_i_qp = start_qp;
            p->start_qp = start_qp;
            p->cur_scale_qp = qp_scale;

            if (p->usr_cfg.i_quality_delta && !p->reenc_cnt) {
                RK_U8 index = mpp_data_mean_v2(p->madi) / 4;
                index = mpp_clip(index, 0, 7);
                dealt_qp = max_ip_qp_dealt[index];
                if (dealt_qp > p->usr_cfg.i_quality_delta ) {
                    dealt_qp = p->usr_cfg.i_quality_delta;
                }
            }

            dealt_qp = mpp_clip(dealt_qp, 5, p->usr_cfg.i_quality_delta);
            if (p->usr_cfg.i_quality_delta) {
                p->start_qp -= dealt_qp;
            }
        } else {
            qp_scale = mpp_clip(qp_scale, (info->quality_min << 6), (info->quality_max << 6));
            p->cur_scale_qp = qp_scale;
            p->start_qp = qp_scale >> 6;
            if (frm->ref_mode == REF_TO_PREV_INTRA && p->usr_cfg.vi_quality_delta) {
                p->start_qp -= p->usr_cfg.vi_quality_delta;
            }
        }
        rc_dbg_rc("i_quality_delta %d, vi_quality_delta %d", dealt_qp, p->usr_cfg.vi_quality_delta);
    }

    p->start_qp = mpp_clip(p->start_qp, info->quality_min, info->quality_max);
    info->quality_target = p->start_qp;

    rc_dbg_rc("bitrate [%d : %d : %d] -> [%d : %d : %d]\n",
              bit_min, bit_target, bit_max,
              info->bit_min, info->bit_target, info->bit_max);
    rc_dbg_rc("quality [%d : %d : %d] -> [%d : %d : %d]\n",
              quality_min, quality_target, quality_max,
              info->quality_min, info->quality_target, info->quality_max);

    rc_dbg_func("leave %p\n", p);
    return MPP_OK;
}

MPP_RET rc_model_v2_hal_end(void *ctx, EncRcTask *task)
{
    rc_dbg_func("enter ctx %p task %p\n", ctx, task);
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

MPP_RET rc_model_v2_check_reenc(void *ctx, EncRcTask *task)
{
    RcModelV2Ctx *p = (RcModelV2Ctx *)ctx;
    EncRcTaskInfo *cfg = (EncRcTaskInfo *)&task->info;
    EncFrmStatus *frm = &task->frm;
    RcCfg *usr_cfg = &p->usr_cfg;

    rc_dbg_func("enter ctx %p cfg %p\n", ctx, cfg);

    frm->reencode = 0;

    if ((usr_cfg->mode == RC_FIXQP) ||
        (task->force.force_flag & ENC_RC_FORCE_QP) ||
        p->on_drop || p->on_pskip)
        return MPP_OK;

    if (check_re_enc(p, cfg)) {
        MppEncRcDropFrmMode drop_mode = usr_cfg->drop_mode;

        if (frm->is_intra)
            drop_mode = MPP_ENC_RC_DROP_FRM_DISABLED;

        if (usr_cfg->drop_gap && p->drop_cnt >= usr_cfg->drop_gap)
            drop_mode = MPP_ENC_RC_DROP_FRM_DISABLED;

        rc_dbg_drop("reenc drop_mode %d drop_cnt %d\n", drop_mode, p->drop_cnt);

        switch (drop_mode) {
        case MPP_ENC_RC_DROP_FRM_NORMAL : {
            frm->drop = 1;
            frm->reencode = 1;
            p->on_drop = 1;
            p->drop_cnt++;
            rc_dbg_drop("drop\n");
        } break;
        case MPP_ENC_RC_DROP_FRM_PSKIP : {
            frm->force_pskip = 1;
            frm->reencode = 1;
            p->on_pskip = 1;
            p->drop_cnt++;
            rc_dbg_drop("force_pskip\n");
        } break;
        case MPP_ENC_RC_DROP_FRM_DISABLED :
        default : {
            if (usr_cfg->mode == RC_CBR) {
                reenc_calc_cbr_ratio(p, cfg);
            } else {
                reenc_calc_vbr_ratio(p, cfg);
            }

            if (p->next_ratio != 0 && cfg->quality_target < cfg->quality_max) {
                p->reenc_cnt++;
                frm->reencode = 1;
            }
            p->drop_cnt = 0;
        } break;
        }
    }

    return MPP_OK;
}

MPP_RET rc_model_v2_end(void *ctx, EncRcTask *task)
{
    RcModelV2Ctx *p = (RcModelV2Ctx *)ctx;
    EncRcTaskInfo *cfg = (EncRcTaskInfo *)&task->info;

    rc_dbg_func("enter ctx %p cfg %p\n", ctx, cfg);

    rc_dbg_rc("bits_mode_update real_bit %d", cfg->bit_real);

    if (p->usr_cfg.mode == RC_FIXQP)
        goto DONE;

    p->last_inst_bps = p->ins_bps;
    p->first_frm_flg = 0;

    bits_model_update(p, cfg->bit_real, cfg->madi);
    if (p->usr_cfg.mode == RC_AVBR) {
        moving_judge_update(p, cfg);
        bit_statics_update(p, cfg->bit_real);
    }

    p->last_frame_type = p->frame_type;
    p->pre_mean_qp = cfg->quality_real;
    p->scale_qp = p->cur_scale_qp;
    p->prev_md_prop = 0;
    p->pre_target_bits = cfg->bit_target;
    p->pre_real_bits = cfg->bit_real;

    p->on_drop = 0;
    p->on_pskip = 0;

DONE:
    rc_dbg_func("leave %p\n", ctx);
    return MPP_OK;
}

const RcImplApi default_h264e = {
    "default",
    MPP_VIDEO_CodingAVC,
    sizeof(RcModelV2Ctx),
    rc_model_v2_init,
    rc_model_v2_deinit,
    NULL,
    rc_model_v2_check_reenc,
    rc_model_v2_start,
    rc_model_v2_end,
    rc_model_v2_hal_start,
    rc_model_v2_hal_end,
};

const RcImplApi default_h265e = {
    "default",
    MPP_VIDEO_CodingHEVC,
    sizeof(RcModelV2Ctx),
    rc_model_v2_init,
    rc_model_v2_deinit,
    NULL,
    rc_model_v2_check_reenc,
    rc_model_v2_start,
    rc_model_v2_end,
    rc_model_v2_hal_start,
    rc_model_v2_hal_end,
};

const RcImplApi default_jpege = {
    "default",
    MPP_VIDEO_CodingMJPEG,
    sizeof(RcModelV2Ctx),
    rc_model_v2_init,
    rc_model_v2_deinit,
    NULL,
    rc_model_v2_check_reenc,
    rc_model_v2_start,
    rc_model_v2_end,
    rc_model_v2_hal_start,
    rc_model_v2_hal_end,
};

static RK_S32 vp8_initial_qp(RK_S32 bits, RK_S32 pels)
{
    RK_S32 i = -1;
    static const RK_S32 qp_tbl[2][12] = {
        {47, 57, 73, 93, 122, 155, 214, 294, 373, 506, 781, 0x7FFFFFFF},
        {120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10}
    };

    if (bits > 1000000)
        return 10;

    pels >>= 8;
    bits >>= 5;

    bits *= pels + 250;
    bits /= 350 + (3 * pels) / 4;
    bits = axb_div_c(bits, UPSCALE, pels << 6);

    while (qp_tbl[0][++i] < bits);

    return qp_tbl[1][i];
}

MPP_RET rc_model_v2_vp8_hal_start(void *ctx, EncRcTask *task)
{
    RcModelV2Ctx *p = (RcModelV2Ctx *)ctx;
    EncFrmStatus *frm = &task->frm;
    EncRcTaskInfo *info = &task->info;
    EncRcForceCfg *force = &task->force;
    RK_S32 mb_w = MPP_ALIGN(p->usr_cfg.width, 16) / 16;
    RK_S32 mb_h = MPP_ALIGN(p->usr_cfg.height, 16) / 16;
    RK_S32 bit_min = info->bit_min;
    RK_S32 bit_max = info->bit_max;
    RK_S32 bit_target = info->bit_target;
    RK_S32 quality_min = info->quality_min;
    RK_S32 quality_max = info->quality_max;
    RK_S32 quality_target = info->quality_target;

    rc_dbg_func("enter p %p task %p\n", p, task);

    rc_dbg_rc("seq_idx %d intra %d\n", frm->seq_idx, frm->is_intra);

    if (force->force_flag & ENC_RC_FORCE_QP) {
        RK_S32 qp = force->force_qp;
        info->quality_target = qp;
        info->quality_max = qp;
        info->quality_min = qp;
        return MPP_OK;
    }

    if (p->usr_cfg.mode == RC_FIXQP)
        return MPP_OK;

    /* setup quality parameters */
    if (p->first_frm_flg && frm->is_intra) {
        if (info->quality_target < 0) {
            if (info->bit_target) {
                p->start_qp = vp8_initial_qp(info->bit_target, mb_w * mb_h * 16 * 16);
                p->cur_scale_qp = (p->start_qp) << 6;
            } else {
                mpp_log("fix qp case but init qp no set");
                info->quality_target = 40;
                p->start_qp = 40;
                p->cur_scale_qp = (p->start_qp) << 6;
            }
        } else {
            p->start_qp = info->quality_target;
            p->cur_scale_qp = (p->start_qp) << 6;
        }

        if (p->reenc_cnt > 0) {
            p->cur_scale_qp += p->next_ratio;
            p->start_qp = p->cur_scale_qp >> 6;
            rc_dbg_rc("p->start_qp = %d, p->cur_scale_qp %d,p->next_ratio %d ", p->start_qp, p->cur_scale_qp, p->next_ratio);
        } else {
            p->start_qp -= p->usr_cfg.i_quality_delta;
        }
        p->cur_scale_qp = mpp_clip(p->cur_scale_qp, (info->quality_min << 6), (info->quality_max << 6));
        p->pre_i_qp = p->cur_scale_qp >> 6;
        p->pre_p_qp = p->cur_scale_qp >> 6;
    } else {
        RK_S32 qp_scale = p->cur_scale_qp + p->next_ratio;
        RK_S32 start_qp = 0;
        RK_S32 dealt_qp = 0;
        if (frm->is_intra) {
            qp_scale = mpp_clip(qp_scale, (info->quality_min << 6), (info->quality_max << 6));

            start_qp = ((p->pre_i_qp + ((qp_scale + p->next_i_ratio) >> 6)) >> 1);

            start_qp = mpp_clip(start_qp, info->quality_min, info->quality_max);
            p->pre_i_qp = start_qp;
            p->start_qp = start_qp;
            p->cur_scale_qp = qp_scale;

            if (p->usr_cfg.i_quality_delta && !p->reenc_cnt) {
                RK_U8 index = mpp_data_mean_v2(p->madi) / 4;
                index = mpp_clip(index, 0, 7);
                dealt_qp = max_ip_qp_dealt[index];
                if (dealt_qp > p->usr_cfg.i_quality_delta ) {
                    dealt_qp = p->usr_cfg.i_quality_delta;
                }
            }

            if (p->usr_cfg.i_quality_delta) {
                p->start_qp -= dealt_qp;
            }
        } else {
            qp_scale = mpp_clip(qp_scale, (info->quality_min << 6), (info->quality_max << 6));
            p->cur_scale_qp = qp_scale;
            p->start_qp = qp_scale >> 6;
            if (frm->ref_mode == REF_TO_PREV_INTRA && p->usr_cfg.vi_quality_delta) {
                p->start_qp -= p->usr_cfg.vi_quality_delta;
            }
        }
        rc_dbg_rc("i_quality_delta %d, vi_quality_delta %d", dealt_qp, p->usr_cfg.vi_quality_delta);
    }

    p->start_qp = mpp_clip(p->start_qp, info->quality_min, info->quality_max);
    info->quality_target = p->start_qp;

    rc_dbg_rc("bitrate [%d : %d : %d] -> [%d : %d : %d]\n",
              bit_min, bit_target, bit_max,
              info->bit_min, info->bit_target, info->bit_max);
    rc_dbg_rc("quality [%d : %d : %d] -> [%d : %d : %d]\n",
              quality_min, quality_target, quality_max,
              info->quality_min, info->quality_target, info->quality_max);

    rc_dbg_func("leave %p\n", p);
    return MPP_OK;
}

const RcImplApi default_vp8e = {
    "default",
    MPP_VIDEO_CodingVP8,
    sizeof(RcModelV2Ctx),
    rc_model_v2_init,
    rc_model_v2_deinit,
    NULL,
    rc_model_v2_check_reenc,
    rc_model_v2_start,
    rc_model_v2_end,
    rc_model_v2_vp8_hal_start,
    rc_model_v2_hal_end,
};
