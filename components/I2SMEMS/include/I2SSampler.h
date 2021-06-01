#ifndef __sampler_base_h__
#define __sampler_base_h__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "RingBuffer.h"

#define AUDIO_BUFFER_COUNT  11

/**
 * Base Class for both the ADC and I2S sampler
 **/
class I2SSampler
{
private:
    AudioBuffer *m_audio_buffers[AUDIO_BUFFER_COUNT];
    RingBufferAccessor *m_write_ring_buffer_accessor;

    // I2S reader task
    TaskHandle_t m_readerTaskHandle;
    // writer task
    TaskHandle_t m_process_task_handle;
    // i2s reader queue
    QueueHandle_t m_i2sQueue;
    // i2s port
    i2s_port_t m_i2sPort;

protected:
    void addSample(int16_t sample);
    virtual void configureI2S() = 0;
    virtual void processI2SData(uint8_t *i2sData, size_t bytesRead) = 0;
    i2s_port_t getI2SPort()
    {
        return m_i2sPort;
    }

public:
    I2SSampler();
    void start(i2s_port_t i2s_port, i2s_config_t &i2s_config, TaskHandle_t processor_task_handle);
    void start(i2s_port_t i2s_port, TaskHandle_t processor_task_handle);
    RingBufferAccessor *getRingBufferReader();

    int getCurrWritePos()
    {
        return m_write_ring_buffer_accessor->getIndex();
    }
    int getRingBufSize()
    {
        return AUDIO_BUFFER_COUNT * SAMPLE_BUFFER_SIZE;
    }
    
    friend void i2sReaderTask(void *param);
};

#endif