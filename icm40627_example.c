/***************************************************************************/ /**
 * @file icm40627_example.c
 * @brief ICM40627 example APIs with vibration threshold buzzer alert
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 */

#include "rsi_debug.h"
#include "sl_si91x_icm40627.h"
#include "icm40627_example.h"
#include "sl_sleeptimer.h"
#include "sl_si91x_ssi.h"
#include "rsi_rom_clks.h"
#include "sl_status.h"
#include "sl_si91x_driver_gpio.h"

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/

#define DELAY_PERIODIC_MS1 500

/* Buzzer connected to BRD2605A Pin 22 / GPIO_11 */
#define BUZZER_PORT SL_GPIO_PORT_A
#define BUZZER_PIN  GPIO_PIN_NUMBER11

/* On-board RGB LED */
#define RED_LED_PORT   SL_GPIO_PORT_D
#define RED_LED_PIN    GPIO_PIN_NUMBER2

#define GREEN_LED_PORT SL_GPIO_PORT_D
#define GREEN_LED_PIN  GPIO_PIN_NUMBER3

#define VIBRATION_THRESHOLD 1.05f

/*******************************************************************************
 ******************************  Data Types  ***********************************
 ******************************************************************************/

/*******************************************************************************
 *************************** LOCAL VARIABLES   *********************************
 ******************************************************************************/

sl_sleeptimer_timer_handle_t timer1;
boolean_t delay_timeout;

static sl_ssi_handle_t ssi_driver_handle = NULL;
static uint32_t ssi_slave_number = SSI_SLAVE_0;

/* Buzzer GPIO configuration */
static sl_si91x_gpio_pin_config_t buzzer_pin_config =
{
  { BUZZER_PORT, BUZZER_PIN },
  GPIO_OUTPUT
};
/* Red LED GPIO configuration */
static sl_si91x_gpio_pin_config_t red_led_pin_config =
{
  { RED_LED_PORT, RED_LED_PIN },
  GPIO_OUTPUT
};

/* Green LED GPIO configuration */
static sl_si91x_gpio_pin_config_t green_led_pin_config =
{
  { GREEN_LED_PORT, GREEN_LED_PIN },
  GPIO_OUTPUT
};

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/

static void on_timeout_timer1(sl_sleeptimer_timer_handle_t *handle, void *data);
static sl_status_t enable_icm40627(bool connect);

/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/

/*******************************************************************************
 * ICM40627 example initialization function
 ******************************************************************************/

void icm40627_example_init(void)
{
  sl_status_t sl_status;
  uint8_t dev_id;

  /***************************************************************************
   * Initialize Buzzer GPIO
   ***************************************************************************/

  sl_status = sl_gpio_driver_init();

if (sl_status != SL_STATUS_OK) {

  DEBUGOUT("GPIO driver init failed\n");

} else {

  /* Configure Buzzer */
  sl_status = sl_gpio_set_configuration(buzzer_pin_config);

  if (sl_status != SL_STATUS_OK) {

    DEBUGOUT("Buzzer GPIO configuration failed\n");

  } else {

    /* Buzzer OFF initially */
    sl_gpio_driver_clear_pin(
      &buzzer_pin_config.port_pin);

    DEBUGOUT("Buzzer GPIO initialized successfully\n");
  }


  /* Configure Red LED */
  sl_status = sl_gpio_set_configuration(red_led_pin_config);

  if (sl_status != SL_STATUS_OK) {

    DEBUGOUT("Red LED GPIO configuration failed\n");

  } else {

    /* Red LED OFF initially */
    sl_gpio_driver_set_pin(
      &red_led_pin_config.port_pin);

    DEBUGOUT("Red LED initialized successfully\n");
  }


  /* Configure Green LED */
  sl_status = sl_gpio_set_configuration(green_led_pin_config);

  if (sl_status != SL_STATUS_OK) {

    DEBUGOUT("Green LED GPIO configuration failed\n");

  } else {

    /* Green LED OFF initially */
    sl_gpio_driver_set_pin(
      &green_led_pin_config.port_pin);

    DEBUGOUT("Green LED initialized successfully\n");
  }
}
  /***************************************************************************
   * Initialize ICM-40627
   ***************************************************************************/

  do {

    // Enable the sensor
    sl_status = enable_icm40627(true);

    if (sl_status != SL_STATUS_OK) {

      DEBUGOUT("ICM40627 enable failed, Error Code: 0x%ld \n",
               sl_status);

      break;

    } else {

      DEBUGOUT("ICM40627 enable successful\n");
    }

    // SSI interface init
    sl_status =
      sl_si91x_icm40627_ssi_interface_init(&ssi_driver_handle,
                                           ssi_slave_number);

    if (sl_status != SL_STATUS_OK) {

      DEBUGOUT("ICM40627 SSI interface init failed, Error Code: 0x%ld \n",
               sl_status);

      break;

    } else {

      DEBUGOUT("ICM40627 SSI interface init successful\n");
    }

    /*************************************************************************
     * Start periodic timer
     *
     * 200 ms = sensor is checked approximately 5 times per second
     *************************************************************************/

    sl_sleeptimer_start_periodic_timer_ms(
      &timer1,
      DELAY_PERIODIC_MS1,
      on_timeout_timer1,
      NULL,
      0,
      SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);

    // Reset the sensor
    sl_status =
      sl_si91x_icm40627_software_reset(ssi_driver_handle);

    if (sl_status != SL_STATUS_OK) {

      DEBUGOUT("ICM40627 software reset un-successful, Error Code: 0x%ld \n",
               sl_status);

      break;

    } else {

      DEBUGOUT("ICM40627 software reset successful\n");
    }

    // Read Who Am I register
    sl_status =
      sl_si91x_icm40627_get_device_id(ssi_driver_handle, &dev_id);

    if ((sl_status == SL_STATUS_OK) &&
        (dev_id == ICM40627_DEVICE_ID)) {

      DEBUGOUT("ICM40627 device ID verification successful\n");

    } else {

      DEBUGOUT("ICM40627 device ID verification failed\n");

      break;
    }

    // Initialize sensor
    sl_status =
      sl_si91x_icm40627_init(ssi_driver_handle);

    if (sl_status != SL_STATUS_OK) {

      DEBUGOUT("ICM40627 initialization failed, Error Code: 0x%ld \n",
               sl_status);

      break;

    } else {

      DEBUGOUT("ICM40627 initialization successful\n");
    }

  } while (false);
}

/*******************************************************************************
 * Function runs continuously in while loop
 ******************************************************************************/

void icm40627_example_process_action(void)
{
  sl_status_t status;

  float temperature = 0;
  float sensor_data[3];

  if (delay_timeout == true) {

    delay_timeout = false;

    /*************************************************************************
     * Read temperature
     *************************************************************************/

    status =
      sl_si91x_icm40627_get_temperature_data(
        ssi_driver_handle,
        &temperature);

    if (status != SL_STATUS_OK) {

      DEBUGOUT("Temperature read failed, Error Code: 0x%ld \n",
               status);

    } else {

      DEBUGOUT("Temperature: %0.2lf\n", temperature);
    }

    /*************************************************************************
     * Read accelerometer data
     *************************************************************************/

    status =
      sl_si91x_icm40627_get_accel_data(
        ssi_driver_handle,
        sensor_data);

    if (status != SL_STATUS_OK) {

      DEBUGOUT("Acceleration read failed, Error Code: 0x%ld \n",
               status);

      /* If sensor reading fails, keep buzzer OFF */
      sl_gpio_driver_clear_pin(&buzzer_pin_config.port_pin);

    } else {

      /***********************************************************************
       * Print acceleration
       ***********************************************************************/

      DEBUGOUT("Acceleration: {  ");

      for (int i = 0; i < 3; i++) {

        DEBUGOUT("%0.2f  ", sensor_data[i]);
      }

      DEBUGOUT("}\n");

      /***********************************************************************
       * VIBRATION THRESHOLD CHECK
       *
       * Check X, Y and Z individually.
       ***********************************************************************/

      if ((sensor_data[0] > VIBRATION_THRESHOLD) ||
    (sensor_data[0] < -VIBRATION_THRESHOLD) ||
    (sensor_data[1] > VIBRATION_THRESHOLD) ||
    (sensor_data[1] < -VIBRATION_THRESHOLD) ||
    (sensor_data[2] > VIBRATION_THRESHOLD) ||
    (sensor_data[2] < -VIBRATION_THRESHOLD)) {

  /*********************************************************************
   * HIGH VIBRATION
   *********************************************************************/

  /* Buzzer ON */
  sl_gpio_driver_set_pin(
    &buzzer_pin_config.port_pin);

  /* Red LED ON */
  sl_gpio_driver_clear_pin(
    &red_led_pin_config.port_pin);

  /* Green LED OFF */
  sl_gpio_driver_set_pin(
    &green_led_pin_config.port_pin);

  DEBUGOUT(
    "WARNING: HIGH VIBRATION - RED LED ON - BUZZER ON\n");

} else {

  /*********************************************************************
   * NORMAL VIBRATION
   *********************************************************************/

  /* Buzzer OFF */
  sl_gpio_driver_clear_pin(
    &buzzer_pin_config.port_pin);

  /* Red LED OFF */
  sl_gpio_driver_set_pin(
    &red_led_pin_config.port_pin);

  /* Green LED ON */
  sl_gpio_driver_clear_pin(
    &green_led_pin_config.port_pin);

  DEBUGOUT(
    "NORMAL VIBRATION - GREEN LED ON - BUZZER OFF\n");
}

    /*************************************************************************
     * Read gyroscope data
     *************************************************************************/

    status =
      sl_si91x_icm40627_get_gyro_data(
        ssi_driver_handle,
        sensor_data);

    if (status != SL_STATUS_OK) {

      DEBUGOUT("Gyro read failed, Error Code: 0x%ld \n",
               status);

    } else {

      DEBUGOUT("Gyro: {  ");

      for (int i = 0; i < 3; i++) {

        DEBUGOUT("%0.2f  ", sensor_data[i]);
      }

      DEBUGOUT("}\n\n");
    }
  }
}
}
/***************************************************************************/ /**
 * Sleeptimer timeout callback.
 ******************************************************************************/

static void on_timeout_timer1(
  sl_sleeptimer_timer_handle_t *handle,
  void *data)
{
  (void)&handle;
  (void)&data;

  delay_timeout = true;
}

/*******************************************************************************
 * Function to connect ICM40627 Sensor
 ******************************************************************************/

static sl_status_t enable_icm40627(bool connect)
{
  sl_status_t status;

  if (sl_si91x_gpio_driver_get_uulp_npss_pin(
        SENSOR_ENABLE_GPIO_PIN) != 1) {

    // Enable GPIO ULP_CLK
    status =
      sl_si91x_gpio_driver_enable_clock(
        (sl_si91x_gpio_select_clock_t)ULPCLK_GPIO);

    if (status != SL_STATUS_OK) {
      return status;
    }

    if (connect) {

      // Set NPSS GPIO pin MUX
      status =
        sl_si91x_gpio_driver_set_uulp_npss_pin_mux(
          SENSOR_ENABLE_GPIO_PIN,
          NPSS_GPIO_PIN_MUX_MODE1);

      if (status != SL_STATUS_OK) {
        return status;
      }

      // Set NPSS GPIO pin direction
      status =
        sl_si91x_gpio_driver_set_uulp_npss_direction(
          SENSOR_ENABLE_GPIO_PIN,
          (sl_si91x_gpio_direction_t)GPIO_OUTPUT);

      if (status != SL_STATUS_OK) {
        return status;
      }

      // Set UULP GPIO pin
      status =
        sl_si91x_gpio_driver_set_uulp_npss_pin_value(
          SENSOR_ENABLE_GPIO_PIN,
          SET);

      if (status != SL_STATUS_OK) {
        return status;
      }

    } else {

      // Disable the sensor
      status =
        sl_si91x_gpio_driver_set_uulp_npss_pin_value(
          SENSOR_ENABLE_GPIO_PIN,
          CLR);

      if (status != SL_STATUS_OK) {
        return status;
      }
    }
  }

  return SL_STATUS_OK;
}