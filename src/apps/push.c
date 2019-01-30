#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "hal/hal.h"

#include "libc.h"
#include "main.h"
#include "regfile.h"
#include "shell.h"

void apPUSH(void *pData)
{
	TickType_t		xWake;

	const int		gpio_A = GPIO_HALL_A;
	const int		gpio_B = GPIO_HALL_B;

	int			pushed_A, pushed_B;
	int			value_A, value_B;

	const float		rpm_table[] = {

		3000.f,
		4000.f,
		5000.f,
		6000.f,
		7000.f
	};

#define rpm_table_MAX		(sizeof(rpm_table) / sizeof(rpm_table[0]))

	int			N, rev;
	float			rpm;

	GPIO_set_mode_INPUT(gpio_A);
	GPIO_set_mode_INPUT(gpio_B);

	pushed_A = GPIO_get_VALUE(gpio_A);
	pushed_B = GPIO_get_VALUE(gpio_B);

	xWake = xTaskGetTickCount();

	do {
		/* Clock 100 Hz.
		 * */
		vTaskDelayUntil(&xWake, (TickType_t) 10);

		value_A = GPIO_get_VALUE(gpio_A);
		value_B = GPIO_get_VALUE(gpio_B);

		/* Detect if buttons are pressed.
		 * */
		if (pushed_A != 0 && value_A == 0) {

			pushed_A = 0;

			if (pm.lu_mode == PM_LU_DISABLED) {

				N = 0;
				rev = (pushed_B == 0) ? 1 : 0;

				pm_fsm_req(&pm, PM_STATE_LU_INITIATE);
				pm_wait_for_IDLE();
			}
			else {
				N = (N < rpm_table_MAX - 1) ? N + 1 : 0;
			}

			rpm = (rev != 0) ? - rpm_table[N] : rpm_table[N];
			reg_SET(ID_PM_S_SETPOINT_RPM, &rpm);
		}
		else if (pushed_A == 0 && value_A != 0) {

			pushed_A = 1;
		}

		if (pushed_B != 0 && value_B == 0) {

			pushed_B = 0;

			if (pm.lu_mode != PM_LU_DISABLED) {

				pm_fsm_req(&pm, PM_STATE_LU_SHUTDOWN);
				pm_wait_for_IDLE();
			}
		}
		else if (pushed_B == 0 && value_B != 0) {

			pushed_B = 1;
		}
	}
	while (1);
}

static TaskHandle_t		xHandle;

SH_DEF(ap_push_startup)
{
	if (xHandle == NULL) {

		xTaskCreate(apPUSH, "apPUSH", configMINIMAL_STACK_SIZE, NULL, 1, &xHandle);
	}
}

SH_DEF(ap_push_halt)
{
	if (xHandle != NULL) {

		vTaskDelete(xHandle);
		xHandle = NULL;
	}
}

