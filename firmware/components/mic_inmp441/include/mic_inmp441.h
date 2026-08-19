#define BUFF_SIZE 2048

// Functions declaration
void i2s_read_task(void *args); // Task for reading the microphone
void i2s_mic_init_std(void); // Initialize the I2S interface
void mic_main(void); // Main function to start the microphone reading task