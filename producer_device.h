#ifndef PRODUCER_DEVICE_H
#define PRODUCER_DEVICE_H

// ============================================================================
// producer_device.h — 频谱仪设备操作封装
//
// 封装所有与 htra_api 的交互，使 producer.cpp 不直接依赖设备 API。
// 两套 API 路径:
//   功率谱: Device_Open → SWP_ProfileDeInit → SWP_Configuration → SWP_GetPartialSweep
//   IQ:     Device_Open → IQS_ProfileDeInit → IQS_Configuration → IQS_GetIQStream
// ============================================================================

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // ======================== 不透明句柄 ========================
    typedef struct DeviceContext DeviceContext;
    typedef struct PowerSpectrumState PowerSpectrumState;
    typedef struct IQState IQState;

    // ======================== 功率谱模式 ========================

    /**
     * 打开设备并配置为功率谱模式
     * @param dev_num     设备号 (通常为 0)
     * @param start_hz    起始频率 (Hz)
     * @param stop_hz     终止频率 (Hz)
     * @param rbw_hz      分辨率带宽 (Hz), 典型值 300e3
     * @return 成功返回 PowerSpectrumState*, 失败返回 NULL
     */
    PowerSpectrumState *ps_open_and_config(int dev_num,
                                           double start_hz,
                                           double stop_hz,
                                           double rbw_hz);

    /**
     * 重新配置功率谱模式的频率 (不关闭设备)
     * @return 0=成功, 非0=失败
     */
    int ps_reconfigure(PowerSpectrumState *ps,
                       double start_hz,
                       double stop_hz,
                       double rbw_hz);

    /**
     * 获取一帧完整的功率谱数据
     * @param ps          功率谱状态
     * @param freq_data   输出: 频率数组 (Hz), 调用者分配, 大小 = ps_get_points()
     * @param power_data  输出: 功率谱数组 (dBm), 调用者分配, 大小 = ps_get_points()
     * @return 0=成功, 非0=失败
     */
    int ps_acquire_sweep(PowerSpectrumState *ps,
                         double *freq_data,
                         float *power_data);

    /** 获取总扫描点数 */
    uint32_t ps_get_points(PowerSpectrumState *ps);

    /** 获取分辨率带宽 */
    double ps_get_rbw(PowerSpectrumState *ps);

    /** 获取当前起始/终止频率 */
    void ps_get_frequency(PowerSpectrumState *ps, double *start_hz, double *stop_hz);

    /** 关闭设备并释放资源 */
    void ps_close(PowerSpectrumState *ps);

    // ======================== IQ 模式 ========================

    /**
     * 打开设备并配置为 IQ 模式
     * @param dev_num           设备号
     * @param center_hz         中心频率 (Hz)
     * @param ref_level_dbm     参考电平 (dBm), 典型值 0
     * @param decimate_factor   抽取倍数, 典型值 2
     * @param trigger_length    触发点数, 典型值 16242
     * @return 成功返回 IQState*, 失败返回 NULL
     */
    IQState *iq_open_and_config(int dev_num,
                                double center_hz,
                                double ref_level_dbm,
                                int decimate_factor,
                                uint32_t trigger_length);

    /**
     * 重新配置 IQ 模式的中心频率
     * @return 0=成功, 非0=失败
     */
    int iq_reconfigure(IQState *iq,
                       double center_hz,
                       double ref_level_dbm,
                       int decimate_factor,
                       uint32_t trigger_length);

    /**
     * 获取一帧完整的 IQ 数据 (触发 → 收集所有包 → 停止触发)
     * @param iq          IQ 状态
     * @param iq_buffer   输出: I/Q 交织的 int16 数组, 调用者分配, 大小 = iq_get_total_ints()
     *                    格式: [I0, Q0, I1, Q1, ...]
     * @return 0=成功, 非0=失败
     */
    int iq_acquire_stream(IQState *iq, int16_t *iq_buffer);

    /** 获取 IQ 总采样点数 (I/Q 对数) */
    uint32_t iq_get_samples(IQState *iq);

    /** 获取 IQ 总 int16 个数 (= samples * 2) */
    uint32_t iq_get_total_ints(IQState *iq);

    /** 获取当前中心频率 */
    double iq_get_center_freq(IQState *iq);

    /** 关闭设备并释放资源 */
    void iq_close(IQState *iq);

#ifdef __cplusplus
}
#endif

#endif // PRODUCER_DEVICE_H
