#ifndef Protocentral_ecg_resp_signal_processing_h
#define Protocentral_ecg_resp_signal_processing_h
#include "Arduino.h"

#define TEMPERATURE          0
#define FILTERORDER         161
#define NRCOEFF           (0.992)
#define WAVE_SIZE            1

//******* ecg filter *********
#define MAX_PEAK_TO_SEARCH         5
#define MAXIMA_SEARCH_WINDOW      25
#define MINIMUM_SKIP_WINDOW       30
#define SAMPLING_RATE             125
#define TWO_SEC_SAMPLES      2 * SAMPLING_RATE
#define QRS_THRESHOLD_FRACTION    0.4
#define TRUE                       1
#define FALSE                      0

class ads1292r_processing
{
  public:
    void ECG_FilterProcess(int16_t * WorkingBuff, int16_t * CoeffBuf, int16_t* FilterOut);
    void Filter_CurrentECG_sample(int16_t *CurrAqsSample, int16_t *FilteredOut);
    void Calculate_HeartRate(int16_t CurrSample,volatile uint8_t *Heart_rate, volatile uint8_t *peakflag);
    void QRS_process_buffer(volatile uint8_t *Heart_rate,volatile uint8_t *peakflag);
    void QRS_check_sample_crossing_threshold( uint16_t scaled_result,volatile uint8_t *Heart_rate,volatile uint8_t *peakflag);  
    void Resp_FilterProcess(int16_t * RESP_WorkingBuff, int16_t * CoeffBuf, int16_t* FilterOut);
    void Filter_CurrentRESP_sample(int16_t CurrAqsSample, int16_t * FiltOut);
    void Calculate_RespRate(int16_t CurrSample,volatile uint8_t *RespirationRate);
    void Respiration_Rate_Detection(int16_t Resp_wave,volatile uint8_t *RespirationRate);
};

#endif
