#include "app_sweep.h"
#include "bsp_pwm.h"
#include "bsp_capture.h"
#include "bsp_tick.h"
#include "app_config.h"

/*==============================================================================
 * app_sweep.c
 *-----------------------------------------------------------------------------
 * 拓展3: 自动扫频自检。
 *   从 10Hz 到 200kHz 选至少 10 个点, 每个点:
 *     设置 PWM -> 等捕获稳定 -> 读回实测频率 -> 算误差 -> 记录
 *   最后屏幕显示表格, 全部达标 PASS, 否则 FAIL。
 *
 * 关键点:
 *   - 必须非阻塞: 不能 while 等, 否则触摸/界面全卡住;
 *     用状态机 + TICK_Elapsed, 每次 APP_Loop 推进一小步。
 *   - 每个频点要等"新的捕获数据", 不能拿上一个频点的旧值;
 *     用 g_cap.seq 判断有没有更新。
 *   - 10Hz 一个周期 100ms, 等待时间不能只有几毫秒;
 *     SWEEP_SETTLE_MS 默认 400ms, 约 4 个周期。
 *   - 这个功能需要把 PWM 输出用杜邦线接到捕获输入 (PA3 -> PA0),
 *     否则测不到自己的输出。
 *   - 200kHz 时捕获中断 20万次/秒, 扫频期间界面刷新要放慢。
 *
 * 已实现: SWEEP_Poll 状态机、捕获等待、误差判定和 PASS/FAIL。
 *============================================================================*/

/* 建议频点: 覆盖低频/中频/高频, 共 12 个 (题目要求至少 10 个)。
 * 你可以改成对数分布, 也可以加 1Hz/500kHz, 注意低频等待时间。 */
const u32 g_sweep_points[SWEEP_POINT_COUNT] =
{
    10UL, 50UL, 100UL, 500UL,
    1000UL, 5000UL, 10000UL, 20000UL,
    50000UL, 100000UL,
#if APP_ENABLE_EXT500K
    200000UL, 500000UL
#else
    150000UL, 200000UL
#endif
};

static SWEEP_Row s_rows[SWEEP_POINT_COUNT];
static SWEEP_State s_state = SWEEP_IDLE;
static u8  s_active = 0;
static u8  s_pass = 1;
static u8  s_index = 0;
static u32 s_point_start_ms = 0;
static u32 s_cap_seq_at_set = 0;

void SWEEP_Init(void)
{
    u8 i;

    s_state = SWEEP_IDLE;
    s_active = 0;
    s_pass = 1;
    s_index = 0;
    s_point_start_ms = 0;
    s_cap_seq_at_set = 0;

    for (i = 0; i < SWEEP_POINT_COUNT; i++)
    {
        s_rows[i].set_hz      = 0;
        s_rows[i].meas_hz     = 0;
        s_rows[i].meas_duty_pm = 0;
        s_rows[i].err_x100    = 0;
        s_rows[i].pass        = 0;
    }
}

void SWEEP_Start(void)
{
    if (s_active) return;

    SWEEP_Init();
    s_active = 1;
    s_state  = SWEEP_SET;
    s_index  = 0;
    s_pass   = 1;
    s_point_start_ms = TICK_Now();
}

void SWEEP_Abort(void)
{
    s_active = 0;
    s_state  = SWEEP_IDLE;
}

static void Sweep_FailPoint(u8 index)
{
    s_rows[index].set_hz       = g_sweep_points[index];
    s_rows[index].meas_hz      = 0;
    s_rows[index].meas_duty_pm = 0;
    s_rows[index].err_x100     = 10000;   /* 100.00% */
    s_rows[index].pass         = 0;
    s_pass = 0;
}

void SWEEP_Poll(void)
{
    u32 now;
    u32 set_hz;
    u32 meas_hz;
    u32 diff;
    u32 err;
    SWEEP_Row *row;

    if (!s_active) return;

    now = TICK_Now();

    switch (s_state)
    {
    case SWEEP_SET:
        if (s_index >= SWEEP_POINT_COUNT)
        {
            s_state = SWEEP_DONE;
            s_active = 0;
            break;
        }

        set_hz = g_sweep_points[s_index];

        if (PWM_Set(set_hz, 50) != APP_OK)
        {
            Sweep_FailPoint(s_index);
            s_index++;
            break;
        }

        PWM_Enable(1);

        /* 记录设置前的捕获序号, 后面必须等到新数据 */
        s_cap_seq_at_set = g_cap.seq;
        s_point_start_ms = now;
        s_state = SWEEP_WAIT;
        break;

    case SWEEP_WAIT:
        /* 至少等 SWEEP_SETTLE_MS, 且必须看到新的捕获序号和有效数据 */
        if ((u32)(now - s_point_start_ms) >= SWEEP_SETTLE_MS &&
            g_cap.seq != s_cap_seq_at_set &&
            CAP_IsValid())
        {
            s_state = SWEEP_MEASURE;
        }
        else if ((u32)(now - s_point_start_ms) >= SWEEP_TIMEOUT_MS)
        {
            Sweep_FailPoint(s_index);
            s_index++;
            s_state = SWEEP_SET;
        }
        break;

    case SWEEP_MEASURE:
        row = &s_rows[s_index];
        row->set_hz       = g_sweep_points[s_index];
        row->meas_hz      = g_cap.freq_hz;
        row->meas_duty_pm = g_cap.duty_permille;

        set_hz  = row->set_hz;
        meas_hz = row->meas_hz;

        if (set_hz == 0)
        {
            row->err_x100 = 10000;
            row->pass = 0;
        }
        else
        {
            diff = (meas_hz > set_hz) ? (meas_hz - set_hz) : (set_hz - meas_hz);
            err = (u32)(((unsigned long long)diff * 10000ULL + set_hz / 2UL) / set_hz);
            if (err > 10000UL) err = 10000UL;

            row->err_x100 = (int32_t)err;
            row->pass = (err <= (u32)(SWEEP_FREQ_TOL_PCT * 100)) ? 1 : 0;
        }

        if (!row->pass) s_pass = 0;

        s_index++;
        s_state = SWEEP_SET;
        break;

    case SWEEP_DONE:
    case SWEEP_IDLE:
    default:
        break;
    }
}

u8 SWEEP_IsActive(void)
{
    return s_active;
}

u8 SWEEP_IsDone(void)
{
    return (s_state == SWEEP_DONE) ? 1 : 0;
}

u8 SWEEP_GetPass(void)
{
    return s_pass;
}

u8 SWEEP_GetCount(void)
{
    return s_index;
}

const SWEEP_Row *SWEEP_GetRows(void)
{
    return s_rows;
}
