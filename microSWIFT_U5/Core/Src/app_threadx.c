/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  ******************************************************************************
  *
  *TODO: update macros to be adjustable by some config.h file
  *TODO: update array size macros once known
  *TODO: write error handler function and call it in:
  	  	  - self test
  	  	  - hard fault
  	  	  - any other condition where bad things happen
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
 typedef enum thread_priorities{
 	HIGHEST = 0,
 	VERY_HIGH = 1,
 	HIGH = 2,
 	MID= 3,
 	LOW = 4,
 	LOWEST = 5
 }thread_priorities_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define THREAD_EXTRA_LARGE_STACK_SIZE 4096
#define THREAD_LARGE_STACK_SIZE 2048
#define THREAD_MEDIUM_STACK_SIZE 1024
#define THREAD_SMALL_STACK_SIZE 512
#define UBX_QUEUE_SIZE 3
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
uint32_t start_time_millis;

extern DMA_QListTypeDef GNSS_LL_Queue;
// The configuration struct
microSWIFT_configuration configuration;
// The SBD message we'll assemble
sbd_message_type_52 sbd_message;
// The primary byte pool from which all memory is allocated from
TX_BYTE_POOL *byte_pool;
// Our threads
TX_THREAD startup_thread;
TX_THREAD gnss_thread;
TX_THREAD imu_thread;
TX_THREAD ct_thread;
TX_THREAD waves_thread;
TX_THREAD iridium_thread;
TX_THREAD teardown_thread;
// We'll use flags to signal other threads to run/shutdown
TX_EVENT_FLAGS_GROUP thread_flags;
// The data structures for Waves
emxArray_real32_T *north;
emxArray_real32_T *east;
emxArray_real32_T *down;
//float* down;
//float* east;
//float* north;
// The primary DMA buffer for GNSS UBX messages
uint8_t* ubx_DMA_message_buf;
// Buffer for messages ready to process
uint8_t* ubx_message_process_buf;
// Iridium buffers
uint8_t* iridium_message;
uint8_t* iridium_response_message;
uint8_t* iridium_error_message;
// GNSS and Iridium structs
GNSS* gnss;
Iridium* iridium;
// Handles for all the STM32 peripherals
device_handles_t *device_handles;
// Only included if we will be using the IMU
#if IMU_ENABLED
int16_t* IMU_N_Array;
int16_t* IMU_E_Array;
int16_t* IMU_D_Array;
#endif
// Only included if there is a CT sensor
#if CT_ENABLED
CT* ct;
CHAR* ct_data;
ct_samples* samples_buf;
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void startup_thread_entry(ULONG thread_input);
void gnss_thread_entry(ULONG thread_input);
void waves_thread_entry(ULONG thread_input);
void iridium_thread_entry(ULONG thread_input);
void teardown_thread_entry(ULONG thread_input);
static void shut_it_all_down(void);
// callback function to get GNSS DMA started
gnss_error_code_t start_GNSS_UART_DMA(GNSS* gnss_struct_ptr, uint8_t* buffer, size_t buffer_size);

#if IMU_ENABLED
void imu_thread_entry(ULONG thread_input);
#endif

#if CT_ENABLED
void ct_thread_entry(ULONG thread_input);
#endif
// Static helper functions
static void led_sequence(uint8_t sequence);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

   /* USER CODE BEGIN App_ThreadX_MEM_POOL */
  start_time_millis = HAL_GetTick();
	(void)byte_pool;
	CHAR *pointer = TX_NULL;

	//
	// Allocate stack for the startup thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_EXTRA_LARGE_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the startup thread. HIGHEST priority level and no preemption possible
	ret = tx_thread_create(&startup_thread, "startup thread", startup_thread_entry, 0, pointer,
			THREAD_EXTRA_LARGE_STACK_SIZE, HIGHEST, HIGHEST, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// Allocate stack for the gnss thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_EXTRA_LARGE_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the gnss thread. VERY_HIGH priority, no preemption-threshold
	ret = tx_thread_create(&gnss_thread, "gnss thread", gnss_thread_entry, 0, pointer,
		  THREAD_EXTRA_LARGE_STACK_SIZE, VERY_HIGH, VERY_HIGH, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// Allocate stack for the waves thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_EXTRA_LARGE_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the waves thread. MID priority, no preemption-threshold
	ret = tx_thread_create(&waves_thread, "waves thread", waves_thread_entry, 0, pointer,
			THREAD_EXTRA_LARGE_STACK_SIZE, MID, MID, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// Allocate stack for the Iridium thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_LARGE_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the Iridium thread. VERY_HIGH priority, no preemption-threshold
	ret = tx_thread_create(&iridium_thread, "iridium thread", iridium_thread_entry, 0, pointer,
			THREAD_LARGE_STACK_SIZE, MID, MID, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// Allocate stack for the teardown thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_SMALL_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the teardown thread. HIGHEST priority, no preemption-threshold
	ret = tx_thread_create(&teardown_thread, "teardown thread", teardown_thread_entry, 0, pointer,
		  THREAD_SMALL_STACK_SIZE, LOWEST, LOWEST, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// Create the event flags we'll use for triggering threads
	ret = tx_event_flags_create(&thread_flags, "thread_flags");
	if (ret != TX_SUCCESS) {
	  return ret;
	}
	//
	// The UBX message array
	ret = tx_byte_allocate(byte_pool, (VOID**) &ubx_DMA_message_buf, UBX_MESSAGE_SIZE * 2, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// The UBX process buffer
	ret = tx_byte_allocate(byte_pool, (VOID**) &ubx_message_process_buf, UBX_MESSAGE_SIZE * 2, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// The Iridium message array -- add 2 to the size for the checksum
	ret = tx_byte_allocate(byte_pool, (VOID**) &iridium_message, IRIDIUM_MESSAGE_PAYLOAD_SIZE + IRIDIUM_CHECKSUM_LENGTH,
			TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// The Iridium error message payload array
	ret = tx_byte_allocate(byte_pool, (VOID**) &iridium_error_message, IRIDIUM_ERROR_MESSAGE_PAYLOAD_SIZE + IRIDIUM_CHECKSUM_LENGTH,
				TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// The Iridium response message array
	ret = tx_byte_allocate(byte_pool, (VOID**) &iridium_response_message, IRIDIUM_MAX_RESPONSE_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// The gnss struct
	ret = tx_byte_allocate(byte_pool, (VOID**) &gnss, sizeof(GNSS), TX_NO_WAIT);
	if (ret != TX_SUCCESS){
		return ret;
	}
	//
	// The iridium struct
	ret = tx_byte_allocate(byte_pool, (VOID**) &iridium, sizeof(Iridium), TX_NO_WAIT);
	if (ret != TX_SUCCESS){
		return ret;
	}
// Only if the IMU will be utilized
#if IMU_ENABLED
	//
	// Allocate stack for the imu thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_LARGE_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the imu thread. VERY_HIGH priority, no preemption-threshold
	ret = tx_thread_create(&imu_thread, "imu thread", imu_thread_entry, 0, pointer,
		  THREAD_LARGE_STACK_SIZE, HIGH, HIGH, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}

	ret = tx_byte_allocate(byte_pool, (VOID**) &IMU_N_Array, SENSOR_DATA_ARRAY_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	ret = tx_byte_allocate(byte_pool, (VOID**) &IMU_E_Array, SENSOR_DATA_ARRAY_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	ret = tx_byte_allocate(byte_pool, (VOID**) &IMU_D_Array, SENSOR_DATA_ARRAY_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}

#endif
// Only is there is a CT sensor present
#if CT_ENABLED
	//
	// Allocate stack for the CT thread
	ret = tx_byte_allocate(byte_pool, (VOID**) &pointer, THREAD_MEDIUM_STACK_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Create the CT thread. VERY_HIGH priority, no preemption-threshold
	ret = tx_thread_create(&ct_thread, "ct thread", ct_thread_entry, 0, pointer,
		  THREAD_MEDIUM_STACK_SIZE, VERY_HIGH, VERY_HIGH, TX_NO_TIME_SLICE, TX_AUTO_START);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	//
	// The ct struct
	ret = tx_byte_allocate(byte_pool, (VOID**) &ct, sizeof(CT), TX_NO_WAIT);
	if (ret != TX_SUCCESS){
		return ret;
	}
	// The CT input data buffer array
	ret = tx_byte_allocate(byte_pool, (VOID**) &ct_data, CT_DATA_ARRAY_SIZE, TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// The CT samples array
	ret = tx_byte_allocate(byte_pool, (VOID**) &samples_buf, TOTAL_CT_SAMPLES * sizeof(ct_samples),
			TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// The CT samples array
	ret = tx_byte_allocate(byte_pool, (VOID**) &sbd_message, sizeof(sbd_message_type_52),
			TX_NO_WAIT);
	if (ret != TX_SUCCESS){
	  return ret;
	}
	// Zero out the sbd message struct
	memset(&sbd_message, 0, sizeof(sbd_message));

	north = argInit_1xUnbounded_real32_T(&configuration); //get_waves_float_array(&configuration);
	east  = argInit_1xUnbounded_real32_T(&configuration); //get_waves_float_array(&configuration);
	down  = argInit_1xUnbounded_real32_T(&configuration); //get_waves_float_array(&configuration);
#endif
  /* USER CODE END App_ThreadX_MEM_POOL */

  /* USER CODE BEGIN App_ThreadX_Init */
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  MX_ThreadX_Init
  * @param  None
  * @retval None
  */
  /* USER CODE BEGIN  Before_Kernel_Start */
void MX_ThreadX_Init(device_handles_t *handles)
{
  device_handles = handles;
  configuration.duty_cycle = DUTY_CYCLE;
  configuration.samples_per_window = TOTAL_SAMPLES_PER_WINDOW;
  configuration.iridium_max_transmit_time = IRIDIUM_MAX_TRANSMIT_TIME;
  configuration.gnss_max_acquisition_wait_time = GNSS_MAX_ACQUISITION_WAIT_TIME;
  configuration.gnss_sampling_rate = GNSS_SAMPLING_RATE;
  configuration.total_ct_samples = TOTAL_CT_SAMPLES;
  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
/**
  * @brief  startup_thread_entry
  *         This thread will start all peripherals and do a systems check to
  *         make sure we're good to start the processing cycle
  * @param  ULONG thread_input - unused
  * @retval void
  */
void startup_thread_entry(ULONG thread_input){
	// TODO: Check flash for first-time flag. If present, skip, otherwise run
	// TODO: set event flags to "ready" for all threads
	UINT threadx_return;
	int fail_counter;

	// Initialize the structs
	gnss_init(gnss, &configuration, device_handles->GNSS_uart, device_handles->GNSS_dma_handle,
			&thread_flags, &(ubx_message_process_buf[0]), device_handles->hrtc, north->data,
			east->data, down->data);
	iridium_init(iridium, &configuration, device_handles->Iridium_uart,
					device_handles->Iridium_rx_dma_handle, device_handles->iridium_timer,
					device_handles->Iridium_tx_dma_handle, &thread_flags,
					device_handles->hrtc, &sbd_message,
					(uint8_t*)iridium_error_message,
					(uint8_t*)iridium_response_message);
#if CT_ENABLED
	ct_init(ct, &configuration, device_handles->CT_uart, device_handles->CT_dma_handle,
					&thread_flags, ct_data, samples_buf);
#endif

	// If the reset was from a software trigger, only test GNSS
	if (device_handles->reset_reason & RCC_RESET_FLAG_SW) {
		///////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////// GNSS STARTUP SEQUENCE /////////////////////////////////////////////
		// turn on the GNSS FET
		gnss->on_off(gnss, GPIO_PIN_SET);
		// Send the configuration commands to the GNSS unit.
		fail_counter = 0;
		while (fail_counter < MAX_SELF_TEST_RETRIES) {
			if (gnss->config(gnss) != GNSS_SUCCESS) {
				// Config didn't work, cycle power and try again
				gnss->cycle_power(gnss);
				fail_counter++;
			} else {
				break;
			}
		}

		if (fail_counter == MAX_SELF_TEST_RETRIES){
			HAL_NVIC_SystemReset();
		}

		// Wait until we get a series of good UBX_NAV_PVT messages and are
		// tracking a good number of satellites before moving on
		fail_counter = 0;
		while (fail_counter < MAX_SELF_TEST_RETRIES) {
			if (gnss->self_test(gnss, start_GNSS_UART_DMA, ubx_DMA_message_buf, UBX_MESSAGE_SIZE)
					!= GNSS_SUCCESS) {
				// self_test failed, cycle power and try again
				gnss->cycle_power(gnss);
				fail_counter++;
			} else {
				gnss->is_configured = true;
				break;
			}

		}

		if (fail_counter == MAX_SELF_TEST_RETRIES){
			HAL_NVIC_SystemReset();
		}

		// If we made it here, the self test passed and we're ready to process messages
		threadx_return = tx_event_flags_set(&thread_flags, GNSS_READY, TX_OR);

		threadx_return = tx_event_flags_set(&thread_flags, IRIDIUM_READY, TX_OR);

#if IMU_ENABLED
		threadx_return = tx_event_flags_set(&thread_flags, IMU_READY, TX_OR);
#endif

#if CT_ENABLED
		threadx_return = tx_event_flags_set(&thread_flags, CT_READY, TX_OR);
#endif
	}

	// This is first time power up, test everything and flash LED sequence
	else {
		///////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////// GNSS STARTUP SEQUENCE /////////////////////////////////////////////
		// turn on the GNSS FET
		gnss->on_off(gnss, GPIO_PIN_SET);
		// Send the configuration commands to the GNSS unit.
		fail_counter = 0;
		while (fail_counter < MAX_SELF_TEST_RETRIES) {
			if (gnss->config(gnss) != GNSS_SUCCESS) {
				// Config didn't work, cycle power and try again
				gnss->cycle_power(gnss);
				fail_counter++;
			} else {
				break;
			}
		}

		if (fail_counter == MAX_SELF_TEST_RETRIES){
			while(1) {
				led_sequence(TEST_CRITICAL_FAULT_LED_SEQUENCE);
			}
		}

		// Wait until we get a series of good UBX_NAV_PVT messages and are
		// tracking a good number of satellites before moving on
		fail_counter = 0;
		while (fail_counter < MAX_SELF_TEST_RETRIES) {
			if (gnss->self_test(gnss, start_GNSS_UART_DMA, ubx_DMA_message_buf, UBX_MESSAGE_SIZE)
					!= GNSS_SUCCESS) {
				// self_test failed, cycle power and try again
				gnss->cycle_power(gnss);
				fail_counter++;
			} else {
				gnss->is_configured = true;
				break;
			}

		}

		if (fail_counter == MAX_SELF_TEST_RETRIES){
			while(1) {
				led_sequence(TEST_CRITICAL_FAULT_LED_SEQUENCE);
			}
		}

		// If we made it here, the self test passed and we're ready to process messages
		threadx_return = tx_event_flags_set(&thread_flags, GNSS_READY, TX_OR);

		///////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////IRIDIUM STARTUP SEQUENCE ///////////////////////////////////////////////
		// Only do this on initial power up, else leave it alone!
		iridium->queue_create(iridium);
		// See if we can get an ack message from the modem
		if (iridium->self_test(iridium) != IRIDIUM_SUCCESS) {
			while(1) {
				led_sequence(TEST_CRITICAL_FAULT_LED_SEQUENCE);
			}
		}
		// Send the configuration settings to the modem
		if (iridium->config(iridium) != IRIDIUM_SUCCESS) {
			while(1) {
				led_sequence(TEST_CRITICAL_FAULT_LED_SEQUENCE);
			}
		}
		// We'll keep power to the modem but put it to sleep
		iridium->sleep(iridium, GPIO_PIN_RESET);

		// We got an ack and were able to config the Iridium modem
		threadx_return = tx_event_flags_set(&thread_flags, IRIDIUM_READY, TX_OR);

#if IMU_ENABLED
		///////////////////////////////////////////////////////////////////////////////////////////////
		////////////////////////// IMU STARTUP SEQUENCE ///////////////////////////////////////////////
#endif

#if CT_ENABLED
		///////////////////////////////////////////////////////////////////////////////////////////////
		/////////////////////////// CT STARTUP SEQUENCE ///////////////////////////////////////////////
		// Make sure we get good data from the CT sensor
		if (ct->self_test(ct, false) != CT_SUCCESS) {
			led_sequence(TEST_NON_CRITICAL_FAULT_LED_SEQUENCE);
		}
		// We can turn off the CT sensor for now
		ct->on_off(ct, GPIO_PIN_RESET);

		// We received a good message from the CT sensor
		tx_event_flags_set(&thread_flags, CT_READY, TX_OR);
#endif

		// If we made it here, everything passed!
		led_sequence(TEST_PASSED_LED_SEQUENCE);
	}

	// We're done, suspend this thread
	tx_thread_suspend(&startup_thread);
}

/**
  * @brief  gnss_thread_entry
  *         Thread that governs the GNSS processing. Note that actuall message processing
  *         happens in interrupt context, so this thread is just acting as the traffic cop.
  *
  * @param  ULONG thread_input - unused
  * @retval void
  */
void gnss_thread_entry(ULONG thread_input){
	ULONG actual_flags;
	uint32_t timeout_start = 0, elapsed_time = 0;
	uint32_t timeout = configuration.gnss_max_acquisition_wait_time * MILLISECONDS_PER_MINUTE;
	// Packed struct fun times alignment
	float __attribute__((aligned(1))) temp_lat = 0;
	float __attribute__((aligned(1))) temp_lon = 0;
	// Wait until we get the ready flag
	tx_event_flags_get(&thread_flags, GNSS_READY, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

	while (!gnss->all_samples_processed) {

		timeout_start = HAL_GetTick();
		while (elapsed_time < timeout) {
			elapsed_time = HAL_GetTick() - timeout_start;
		}

		if (!gnss->all_resolution_stages_complete) {
			// TODO: sleep for 1 hour by RTC
			HAL_Delay(100);
		}
		while (!gnss->all_samples_processed){
			HAL_Delay(1);
		}
	}
	// turn off the GNSS sensor
	gnss->on_off(gnss, GPIO_PIN_RESET);
	// Deinit UART and DMA to prevent spurious interrupts
	HAL_UART_DeInit(gnss->gnss_uart_handle);
	HAL_DMA_DeInit(gnss->gnss_dma_handle);

	gnss->get_location(gnss, &temp_lat, &temp_lon);
	// Just to be overly sure about alignment
	memcpy(&sbd_message.Lat, &temp_lat, sizeof(float));
	memcpy(&sbd_message.Lon, &temp_lon, sizeof(float));
	// We're using the "port" field to encode how many samples were averaged. If the number
	// is greater than 255, then you just get 255.
	uint8_t sbd_port = (gnss->total_samples_averaged > 255) ? 255 : gnss->total_samples_averaged;
	memcpy(&sbd_message.port, &sbd_port, sizeof(uint8_t));

	tx_event_flags_set(&thread_flags, GNSS_DONE, TX_OR);

	tx_thread_suspend(&gnss_thread);
}

#if IMU_ENABLED
/**
  * @brief  imu_thread_entry
  *         Thread that governs the IMU velocity processing and building of
  *         uIMUArray, vIMUArray, zIMUArray arrays.
  * @param  ULONG thread_input - unused
  * @retval void
  */
void imu_thread_entry(ULONG thread_input){
	/*
	 * Currently disabled.
	 * TODO: enable this function
	 */
//	ULONG actual_flags;
//	tx_event_flags_get(&thread_flags, IMU_READY, TX_OR, &actual_flags, TX_WAIT_FOREVER);
	tx_event_flags_set(&thread_flags, IMU_DONE, TX_OR);
}
#endif

#if CT_ENABLED
/**
  * @brief  ct_thread_entry
  *         This thread will handle the CT sensor, capture readings, and getting averages..
  *
  * @param  ULONG thread_input - unused
  * @retval void
  */
void ct_thread_entry(ULONG thread_input){
	ULONG actual_flags;
	ct_error_code_t return_code;
	uint32_t ct_parsing_error_counter = 0;

	tx_event_flags_get(&thread_flags, GNSS_DONE, TX_OR, &actual_flags, TX_WAIT_FOREVER);

	// If the CT sensor doesn't respond, set the error flag and quit
	if (ct->self_test(ct, true) != CT_SUCCESS) {
		tx_event_flags_set(&thread_flags, CT_ERROR, TX_OR);
		tx_event_flags_set(&thread_flags, CT_DONE, TX_OR);
		tx_event_flags_set(&thread_flags, WAVES_READY, TX_OR);

#ifdef DBUG
		for (int i = 0; i < 100; i++) {
			led_sequence(TEST_CRITICAL_FAULT_LED_SEQUENCE);
		}
#endif

		tx_thread_suspend(&ct_thread);
	}

	// Take our samples
	while (ct->total_samples < configuration.total_ct_samples) {
		return_code = ct_parse_sample(ct);
		if (return_code == CT_PARSING_ERROR) {
			// If there are too many parsing errors, then there's something wrong
			if (++ct_parsing_error_counter == 10) {
				tx_event_flags_set(&thread_flags, CT_ERROR, TX_OR);
				tx_event_flags_set(&thread_flags, CT_DONE, TX_OR);
				tx_event_flags_set(&thread_flags, WAVES_READY, TX_OR);
#ifdef DBUG
				for (int i = 0; i < 100; i++) {
					led_sequence(TEST_CRITICAL_FAULT_LED_SEQUENCE);
				}
#endif
				tx_thread_suspend(&ct_thread);
			}
		}
	}
	// Turn off the CT sensor
	ct->on_off(ct, GPIO_PIN_RESET);
	// Deinit UART and DMA to prevent spurious interrupts
	HAL_UART_DeInit(ct->ct_uart_handle);
	HAL_DMA_DeInit(ct->ct_dma_handle);

	// Got our samples, now average them
	return_code = ct->get_averages(ct);
	// Make sure something didn't go terribly wrong
	if (return_code == CT_NOT_ENOUGH_SAMPLES) {
		tx_event_flags_set(&thread_flags, CT_ERROR, TX_OR);
		tx_event_flags_set(&thread_flags, CT_DONE, TX_OR);
		tx_thread_suspend(&ct_thread);
	}

	real16_T half_salinity = floatToHalf(ct->averages.salinity);
	real16_T half_temp = floatToHalf(ct->averages.temp);

	memcpy(&sbd_message.mean_salinity, &half_salinity, sizeof(real16_T));
	memcpy(&sbd_message.mean_temp, &half_temp, sizeof(real16_T));

	tx_event_flags_set(&thread_flags, CT_DONE, TX_OR);
	tx_event_flags_set(&thread_flags, WAVES_READY, TX_OR);

	tx_thread_suspend(&ct_thread);
}
#endif

/**
  * @brief  waves_thread_entry
  *         This thread will run the GPSWaves algorithm.
  *
  * @param  ULONG thread_input - unused
  * @retval void
  */
void waves_thread_entry(ULONG thread_input){

	ULONG actual_flags;
	tx_event_flags_get(&thread_flags, WAVES_READY, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER);

	// Function return parameters
	real16_T E[42];
	real16_T Dp;
	real16_T Hs;
	real16_T Tp;
	real16_T b_fmax;
	real16_T b_fmin;
	signed char a1[42];
	signed char a2[42];
	signed char b1[42];
	signed char b2[42];
	unsigned char check[42];

	/* Call the entry-point 'NEDwaves_memlight'. */
	NEDwaves_memlight(north, east, down, gnss->sample_window_freq, &Hs, &Tp, &Dp, E,
					&b_fmin, &b_fmax, a1, b1, a2, b2, check);
	// TODO: create a function to assemble the Iridium payload here, to include
	//       changing the endianess of array E

	emxDestroyArray_real32_T(down);
	emxDestroyArray_real32_T(east);
	emxDestroyArray_real32_T(north);

	memcpy(&sbd_message.Hs, &Hs, sizeof(real16_T));
	memcpy(&sbd_message.Tp, &Tp, sizeof(real16_T));
	memcpy(&sbd_message.Dp, &Dp, sizeof(real16_T));
	memcpy(&(sbd_message.E_array[0]), &(E[0]), 42 * sizeof(real16_T));
	memcpy(&sbd_message.f_min, &b_fmin, sizeof(real16_T));
	memcpy(&sbd_message.f_max, &b_fmax, sizeof(real16_T));
	memcpy(&(sbd_message.a1_array[0]), &(a1[0]), 42 * sizeof(signed char));
	memcpy(&(sbd_message.b1_array[0]), &(b1[0]), 42 * sizeof(signed char));
	memcpy(&(sbd_message.a2_array[0]), &(a2[0]), 42 * sizeof(signed char));
	memcpy(&(sbd_message.b2_array[0]), &(b2[0]), 42 * sizeof(signed char));
	memcpy(&(sbd_message.cf_array[0]), &(check[0]), 42 * sizeof(unsigned char));

	tx_event_flags_set(&thread_flags, WAVES_DONE, TX_OR);

	tx_thread_suspend(&waves_thread);
}

/**
  * @brief  iridium_thread_entry
  *         This thread will handle message sending via Iridium modem.
  *
  * @param  ULONG thread_input - unused
  * @retval void
  */
void iridium_thread_entry(ULONG thread_input){
	ULONG actual_flags;
	iridium_error_code_t return_code;

	tx_event_flags_get(&thread_flags, WAVES_DONE, TX_OR, &actual_flags, TX_WAIT_FOREVER);

	uint8_t ascii_7 = '7';
	uint8_t sbd_type = 52;
	uint16_t sbd_size = 327;
	real16_T sbd_voltage = floatToHalf(6.2);
	float sbd_timestamp = iridium->get_timestamp(iridium);
	// finish filling out the sbd message
	memcpy(&sbd_message.legacy_number_7, &ascii_7, sizeof(uint8_t));
	memcpy(&sbd_message.type, &sbd_type, sizeof(uint8_t));
	memcpy(&sbd_message.size, &sbd_size, sizeof(uint16_t));
	memcpy(&sbd_message.mean_voltage, &sbd_voltage, sizeof(real16_T));
	memcpy(&sbd_message.timestamp, &sbd_timestamp, sizeof(float));

	// This will turn on the modem and make sure the caps are charged
	iridium->self_test(iridium);
	HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);

	memcpy(&iridium->current_lat, &sbd_message.Lat, sizeof(float));
	memcpy(&iridium->current_lon, &sbd_message.Lon, sizeof(float));

	return_code = iridium->transmit_message(iridium);
	// TODO: do something if the return code is not success?

	HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);

	// Turn off the modem
	iridium->sleep(iridium, GPIO_PIN_RESET);
	iridium->on_off(iridium, GPIO_PIN_RESET);
	// Deinit UART and DMA to prevent spurious interrupts
	HAL_UART_DeInit(iridium->iridium_uart_handle);
	HAL_DMA_DeInit(iridium->iridium_rx_dma_handle);
	HAL_DMA_DeInit(iridium->iridium_tx_dma_handle);

#ifdef DBUG
	int32_t elapsed_time = (HAL_GetTick() - start_time_millis) / 1000;
#endif
	tx_event_flags_set(&thread_flags, IRIDIUM_DONE, TX_OR);

	tx_thread_suspend(&iridium_thread);


}

/**
  * @brief  teardown_thread_entry
  *         This thread will execute when either an error flag is set or all
  *         the done flags are set, indicating we are ready to shutdown until
  *         the next window.
  * @param  ULONG thread_input - unused
  * @retval void
  */
void teardown_thread_entry(ULONG thread_input){
	ULONG actual_flags, required_flags;
	required_flags =  	GNSS_DONE | IRIDIUM_DONE | WAVES_DONE;
	RTC_AlarmTypeDef alarm = {0};
	RTC_DateTypeDef rtc_date;
	RTC_TimeTypeDef rtc_time;

#if CT_ENABLED
	required_flags |= CT_DONE;
#endif

#if IMU_ENABLED
	required_flags |= IMU_DONE;
#endif

	tx_event_flags_get(&thread_flags, required_flags, TX_AND_CLEAR, &actual_flags, TX_WAIT_FOREVER);

	// Just to be overly sure everything is off
	shut_it_all_down();

	// Get the date and time
	HAL_RTC_GetTime(device_handles->hrtc, &rtc_time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(device_handles->hrtc, &rtc_date, RTC_FORMAT_BIN);

	alarm.Alarm = RTC_ALARM_A;
	alarm.AlarmDateWeekDay = RTC_WEEKDAY_MONDAY;
	alarm.AlarmTime = rtc_time;
#ifdef DBUG
	alarm.AlarmTime.Minutes = rtc_time.Minutes == 59 ? 1 : rtc_time.Minutes + 2;
#else
	alarm.AlarmTime.Minutes = 0;
#endif
	alarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY | RTC_ALARMMASK_HOURS | RTC_ALARMMASK_SECONDS;
	alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;

	if (HAL_RTC_SetAlarm_IT(device_handles->hrtc, &alarm, RTC_FORMAT_BIN) != HAL_OK)
	{
		HAL_NVIC_SystemReset();
	}
	// Enable RTC interrupt
	// This is being enabled here because of the RTC timer interrupt issue
	// documented in the errata. We don't want any GPDMA, UART, LPUART IRQs
	// to trigger the RTC IRQ
	HAL_NVIC_SetPriority(RTC_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(RTC_IRQn);

	// See errata regarding ICACHE access on wakeup
	HAL_ICACHE_Disable();
	HAL_Delay(1);
	HAL_SuspendTick();

	HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);

	HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

	// Make it easy and just reset
	HAL_NVIC_SystemReset();
}

// TODO: Register callbacks with each respective element
/**
  * @brief  UART ISR callback
  *
  * @param  UART_HandleTypeDef *huart - pointer to the UART handle

  * @retval void
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
//	static uint8_t* read_ptr = &(gnss->ubx_process_buf[0]);
//	static write_ptr = &(gnss->ubx_process_buf[0]);
	HAL_StatusTypeDef HAL_return;
	if (huart->Instance == gnss->gnss_uart_handle->Instance) {
		if (!gnss->is_configured) {

			tx_event_flags_set(&thread_flags, GNSS_CONFIG_RECVD, TX_NO_WAIT);

		} else {
			memcpy(&(gnss->ubx_process_buf[0]), &(ubx_DMA_message_buf[0]), UBX_MESSAGE_SIZE);
			gnss->process_message(gnss);
		}
	}

	// CT sensor
	else if (huart->Instance == ct->ct_uart_handle->Instance) {
		tx_event_flags_set(&thread_flags, CT_MSG_RECVD, TX_OR);
	}

	// Iridium modem
	else if (huart->Instance == iridium->iridium_uart_handle->Instance) {
		tx_event_flags_set(&thread_flags, IRIDIUM_MSG_RECVD, TX_OR);
	}
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM16 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// High frequency, low overhead ISR, no need to save/restore context
//	_tx_thread_context_save();

	if (htim->Instance == TIM16) {
		HAL_IncTick();
	}
	else if (htim->Instance == TIM17) {
		iridium->timer_timeout = true;
	}

//	_tx_thread_context_restore();
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
	// Clear the alarm flag
	HAL_PWR_EnableBkUpAccess();
	WRITE_REG(RTC->SCR, RTC_SCR_CALRAF);
}

/**
  * @brief  Static function to flash a sequence of onboard LEDs to indicate
  * success or failure of self-test.
  *
  * @param  sequence: 	INITIAL_LED_SEQUENCE
  * 					TEST_PASSED_LED_SEQUENCE
  * 					TEST_NON_CIRTICAL_FAULT_LED_SEQUENCE
  * 					TEST_CRITICAL_FAULT_LED_SEQUENCE
  *
  * @retval Void
  */
static void led_sequence(led_sequence_t sequence)
{
	switch (sequence) {
		case INITIAL_LED_SEQUENCE:
			for (int i = 0; i < 5; i++){
				HAL_GPIO_WritePin(GPIOG, LED_RED_Pin, GPIO_PIN_SET);
				HAL_Delay(250);
				HAL_GPIO_WritePin(GPIOB, LED_BLUE_Pin, GPIO_PIN_SET);
				HAL_Delay(250);
				HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
				HAL_Delay(250);
				HAL_GPIO_WritePin(GPIOG, LED_RED_Pin, GPIO_PIN_RESET);
				HAL_Delay(250);
				HAL_GPIO_WritePin(GPIOB, LED_BLUE_Pin, GPIO_PIN_RESET);
				HAL_Delay(250);
				HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
				HAL_Delay(250);
			}
			break;

		case TEST_PASSED_LED_SEQUENCE:
			for (int i = 0; i < 10; i++){
				HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
			}
			break;

		case TEST_NON_CRITICAL_FAULT_LED_SEQUENCE:
			for (int i = 0; i < 10; i++){
				HAL_GPIO_WritePin(GPIOB, LED_BLUE_Pin, GPIO_PIN_SET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(GPIOB, LED_BLUE_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
			}
			break;

		case TEST_CRITICAL_FAULT_LED_SEQUENCE:
			for (int i = 0; i < 10; i++){
				HAL_GPIO_WritePin(GPIOG, LED_RED_Pin, GPIO_PIN_SET);
				HAL_Delay(500);
				HAL_GPIO_WritePin(GPIOG, LED_RED_Pin, GPIO_PIN_RESET);
				HAL_Delay(500);
			}
			break;

		default:
			break;
	}
}

/**
  * @brief  callback function to start GNSS UART DMA reception. Passed to GNSS->self_test
  *
  * @param  uart_handle - handle for uart port
  * 		buffer - buffer to store UBX messages in
  * 		buffer_size - capacity of the bufer
  *
  * @retval GNSS_SUCCESS or
  * 	    GNS_UART_ERROR
  */
gnss_error_code_t start_GNSS_UART_DMA(GNSS* gnss_struct_ptr, uint8_t* buffer, size_t msg_size)
{
	gnss_error_code_t return_code = GNSS_SUCCESS;
	HAL_StatusTypeDef hal_return_code;

	gnss->reset_uart(gnss, GNSS_DEFAULT_BAUD_RATE);

	memset(&(buffer[0]), 0, UBX_MESSAGE_SIZE * 2);

	HAL_UART_DMAStop(gnss_struct_ptr->gnss_uart_handle);

	hal_return_code = MX_GNSS_LL_Queue_Config();

	if (hal_return_code != HAL_OK) {
		return_code = GNSS_UART_ERROR;
	}

	gnss_struct_ptr->gnss_dma_handle->InitLinkedList.Priority = DMA_LOW_PRIORITY_HIGH_WEIGHT;
	gnss_struct_ptr->gnss_dma_handle->InitLinkedList.LinkStepMode = DMA_LSM_FULL_EXECUTION;
	gnss_struct_ptr->gnss_dma_handle->InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
	gnss_struct_ptr->gnss_dma_handle->InitLinkedList.TransferEventMode = DMA_TCEM_LAST_LL_ITEM_TRANSFER;
	gnss_struct_ptr->gnss_dma_handle->InitLinkedList.LinkedListMode = DMA_LINKEDLIST_CIRCULAR;

	if (HAL_DMAEx_List_Init(gnss_struct_ptr->gnss_dma_handle) != HAL_OK)
	{
		return_code = GNSS_UART_ERROR;
	}

	__HAL_LINKDMA(gnss_struct_ptr->gnss_uart_handle, hdmarx, *gnss_struct_ptr->gnss_dma_handle);

	hal_return_code = HAL_DMAEx_List_LinkQ(gnss_struct_ptr->gnss_dma_handle, &GNSS_LL_Queue);
	if (hal_return_code != HAL_OK) {
		return_code = GNSS_UART_ERROR;
	}

	hal_return_code = HAL_UART_Receive_DMA(gnss_struct_ptr->gnss_uart_handle,
			(uint8_t*)&(buffer[0]), msg_size);
		//  No need for the half-transfer complete interrupt, so disable it
	__HAL_DMA_DISABLE_IT(gnss->gnss_dma_handle, DMA_IT_HT);

	if (hal_return_code != HAL_OK) {
		return_code = GNSS_UART_ERROR;
	}

	return return_code;
}

static void shut_it_all_down(void)
{

}
/* USER CODE END 1 */
