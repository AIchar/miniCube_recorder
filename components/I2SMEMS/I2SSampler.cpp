
#include "I2SSampler.h"
#include "driver/i2s.h"
#include <algorithm>
#include "RingBuffer.h"


I2SSampler::I2SSampler(){
    for(int i = 0; i < AUDIO_BUFFER_COUNT; i++){
        m_audio_buffers[i] = new AudioBuffer();
    }
    m_write_ring_buffer_accessor = new RingBufferAccessor(m_audio_buffers, AUDIO_BUFFER_COUNT);

}

void I2SSampler::addSample(int16_t sample)
{
    m_write_ring_buffer_accessor->setCurrentSample(sample);
    if(m_write_ring_buffer_accessor->moveToNextSample()){
        //读完一个小buf
        xTaskNotify(m_process_task_handle, 1, eSetBits);
    }
}

void i2sReaderTask(void *param)
{
    I2SSampler *sampler = (I2SSampler *)param;
    while (true)
    {
        // wait for some data to arrive on the queue
        i2s_event_t evt;
        if (xQueueReceive(sampler->m_i2sQueue, &evt, portMAX_DELAY) == pdPASS)
        {
            if (evt.type == I2S_EVENT_RX_DONE)
            {
                size_t bytesRead = 0;
                do
                {
                    // read data from the I2S peripheral
                    uint8_t i2sData[1024];
                    // read from i2s
                    i2s_read(sampler->getI2SPort(), i2sData, 1024, &bytesRead, 10);
                    // process the raw data
                    sampler->processI2SData(i2sData, bytesRead);
                } while (bytesRead > 0);
            }
        }
    }
}

void I2SSampler::start(i2s_port_t i2sPort, i2s_config_t &i2sConfig, TaskHandle_t processor_task_handle)
{
    m_i2sPort = i2sPort;
    m_process_task_handle = processor_task_handle;
    //install and start i2s driver
    i2s_driver_install(m_i2sPort, &i2sConfig, 4, &m_i2sQueue);
    // set up the I2S configuration from the subclass
    configureI2S();

    // start a task to read samples from the ADC
    xTaskCreatePinnedToCore(i2sReaderTask, "i2s Reader Task", 2048, this, 8, &m_readerTaskHandle, 0);
}


void I2SSampler::start(i2s_port_t i2sPort, TaskHandle_t processor_task_handle)
{
    m_i2sPort = i2sPort;
    m_process_task_handle = processor_task_handle;

    i2s_config_t i2sMemsConfigBothChannels = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0}; 

    //install and start i2s driver
    i2s_driver_install(m_i2sPort, &i2sMemsConfigBothChannels, 4, &m_i2sQueue);
    // set up the I2S configuration from the subclass
    configureI2S();

    // start a task to read samples from the ADC
    xTaskCreatePinnedToCore(i2sReaderTask, "i2s Reader Task", 2048, this, 8, &m_readerTaskHandle, 0);
}


RingBufferAccessor *I2SSampler::getRingBufferReader(){
    RingBufferAccessor *reader = new RingBufferAccessor(m_audio_buffers, AUDIO_BUFFER_COUNT);
    // place the reaader at the same position as the writer - clients can move it around as required
    reader->setIndex(m_write_ring_buffer_accessor->getIndex());
    return reader;
}