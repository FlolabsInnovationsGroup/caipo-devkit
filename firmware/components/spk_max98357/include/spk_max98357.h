#define BUFF_SIZE 2046

// Functions declaration
void i2s_write_task(void *args); // Task for writing to the speaker
void i2s_spk_init_std(void); // Initialize the I2S interface
void spk_main(void); // Main function to start the speaker writing task