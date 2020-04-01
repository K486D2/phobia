#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "hal/hal.h"

#include "libc.h"
#include "main.h"
#include "regfile.h"
#include "shell.h"
#include "tel.h"

#define REG_DEF(l, e, u, f, m, p, t)	{ #l #e "\0" u, f, m, (void *) &l, (void *) p, (void *) t}
#define REG_MAX				(sizeof(regfile) / sizeof(reg_t) - 1UL)

static int		null;
static void		*silent;

static void
silent_putc(int c) { /* Do nothing */ }

static void
reg_proc_silent(const reg_t *reg, int *lval, const int *rval)
{
	void		(** kept) (int c) = (void *) reg->link;

	if (lval != NULL) {

		*lval = (*kept != NULL) ? 1 : 0;
	}
	else if (rval != NULL) {

		if (*rval == 1 && *kept == NULL) {

			*kept = iodef->putc;
			iodef->putc = &silent_putc;
		}
		else if (*rval == 0 && kept != NULL) {

			iodef->putc = *kept;
			*kept = NULL;
		}
	}
}

static void
reg_proc_pwm(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f;
	}
	else if (rval != NULL) {

		reg->link->f = *rval;

		taskENTER_CRITICAL();
		ADC_irq_lock();

		if (pm.lu_mode == PM_LU_DISABLED) {

			PWM_configure();

			pm.freq_hz = hal.PWM_frequency;
			pm.dT = 1.f / pm.freq_hz;
			pm.dc_resolution = hal.PWM_resolution;
		}

		ADC_irq_unlock();
		taskEXIT_CRITICAL();
	}
}

static void
reg_proc_ppm(const reg_t *reg, int *lval, const int *rval)
{
	if (lval != NULL) {

		*lval = reg->link->i;
	}
	else if (rval != NULL) {

		reg->link->i = *rval;

		taskENTER_CRITICAL();
		ADC_irq_lock();

		PPM_configure();

		ADC_irq_unlock();
		taskEXIT_CRITICAL();
	}
}

static void
reg_proc_tim(const reg_t *reg, int *lval, const int *rval)
{
	if (lval != NULL) {

		*lval = reg->link->i;
	}
	else if (rval != NULL) {

		reg->link->i = *rval;

		taskENTER_CRITICAL();
		ADC_irq_lock();

		TIM_configure();

		ADC_irq_unlock();
		taskEXIT_CRITICAL();
	}
}

static void
reg_proc_rpm(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f * (60.f / 2.f / M_PI_F) / pm.const_Zp;
	}
	else if (rval != NULL) {

		reg->link->f = (*rval) * (2.f * M_PI_F / 60.f) * pm.const_Zp;
	}
}

static void
reg_proc_mmps(const reg_t *reg, float *lval, const float *rval)
{
	float			rpm;

	if (lval != NULL) {

		reg_proc_rpm(reg, &rpm, NULL);

		*lval = rpm * pm.const_ld_S * (1000.f / 60.f);
	}
	else if (rval != NULL) {

		if (pm.const_ld_S > M_EPS_F) {

			rpm = (*rval) / pm.const_ld_S * (60.f / 1000.f);

			reg_proc_rpm(reg, NULL, &rpm);
		}
	}
}

static void
reg_proc_kmh(const reg_t *reg, float *lval, const float *rval)
{
	float			rpm;

	if (lval != NULL) {

		reg_proc_rpm(reg, &rpm, NULL);

		*lval = rpm * pm.const_ld_S * (3.6f / 60.f);
	}
	else if (rval != NULL) {

		if (pm.const_ld_S > M_EPS_F) {

			rpm = (*rval) / pm.const_ld_S * (60.f / 3.6f);

			reg_proc_rpm(reg, NULL, &rpm);
		}
	}
}

static void
reg_proc_rpm_pc(const reg_t *reg, float *lval, const float *rval)
{
	float			KPC = PM_EMAX(&pm) / 100.f;

	if (lval != NULL) {

		*lval = reg->link->f * pm.const_E / (KPC * pm.const_fb_U);
	}
	else if (rval != NULL) {

		if (pm.const_E > M_EPS_F) {

			reg->link->f = (*rval) * KPC * pm.const_fb_U / pm.const_E;
		}
	}
}

static void
reg_proc_Q_pc(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f * 100.f / pm.i_maximal;
	}
	else if (rval != NULL) {

		reg->link->f = (*rval) * pm.i_maximal / 100.f;
	}
}

static void
reg_proc_kv(const reg_t *reg, float *lval, const float *rval)
{
        if (lval != NULL) {

                *lval = 5.513289f / (reg->link->f * pm.const_Zp);
        }
        else if (rval != NULL) {

                reg->link->f = 5.513289f / ((*rval) * pm.const_Zp);
        }
}

static void
reg_proc_kgm2(const reg_t *reg, float *lval, const float *rval)
{
	float			Zp2 = (float) (pm.const_Zp * pm.const_Zp);

	if (lval != NULL) {

		*lval = reg->link->f * 1.5f * Zp2 * pm.const_E;
	}
	else if (rval != NULL) {

		if (pm.const_E > M_EPS_F) {

			reg->link->f = (*rval) / (1.5f * Zp2 * pm.const_E);
		}
	}
}

static void
reg_proc_kg(const reg_t *reg, float *lval, const float *rval)
{
	const float		ld_R = pm.const_ld_S / (2.f * M_PI_F);

	if (lval != NULL) {

		if (ld_R > M_EPS_F) {

			*lval = reg->link->f * 1.5f * pm.const_E / (ld_R * ld_R);
		}
		else {
			*lval = 0.f;
		}
	}
	else if (rval != NULL) {

		if (pm.const_E > M_EPS_F) {

			reg->link->f = (*rval) * ld_R * ld_R / (1.5f * pm.const_E);
		}
	}
}

static void
reg_proc_BEMF(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f * pm.lu_MPPE * pm.const_E;
	}
	else if (rval != NULL) {

		if (pm.const_E > M_EPS_F) {

			reg->link->f = (*rval) / (pm.lu_MPPE * pm.const_E);
		}
	}
}

static void
reg_proc_halt(const reg_t *reg, float *lval, const float *rval)
{
	float			halt, adjust;

	if (lval != NULL) {

		*lval = reg->link->f;
	}
	else if (rval != NULL) {

		if (*rval < M_EPS_F) {

			halt = ADC_RESOLUTION * 95E-2f / 2.f;

			adjust = (pm.ad_IA[1] < pm.ad_IB[1])
				? pm.ad_IA[1] : pm.ad_IB[1];
			adjust = (adjust == 0.f) ? 1.f : adjust;

			reg->link->f = (float) (int) (halt * hal.ADC_const.GA * adjust);
		}
		else {
			reg->link->f = *rval;
		}
	}
}

static void
reg_proc_maximal_i(const reg_t *reg, float *lval, const float *rval)
{
	float			range, max_1;

	if (lval != NULL) {

		*lval = reg->link->f;
	}
	else if (rval != NULL) {

		if (*rval < M_EPS_F) {

			range = pm.fault_current_halt * 95E-2f;

			if (pm.const_R > M_EPS_F) {

				max_1 = PM_UMAX(&pm) * pm.const_fb_U / pm.const_R;
				range = (max_1 < range) ? max_1 : range;
			}

			reg->link->f = (float) (int) (range);
		}
		else {
			reg->link->f = *rval;
		}
	}
}

static void
reg_proc_reverse_i(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f;
	}
	else if (rval != NULL) {

		if (*rval < M_EPS_F) {

			reg->link->f = pm.i_maximal;
		}
		else {
			reg->link->f = *rval;
		}
	}
}

static void
reg_proc_watt(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f;
	}
	else if (rval != NULL) {

		if (*rval < M_EPS_F) {

			reg->link->f = 1.f;
		}
		else {
			reg->link->f = *rval;
		}
	}
}

static void
reg_proc_F_g(const reg_t *reg, float *lval, const float *rval)
{
	float			*F = (void *) reg->link;
	float			f_cosine, f_sine;

        if (lval != NULL) {

		taskENTER_CRITICAL();
		ADC_irq_lock();

		f_cosine = F[0];
		f_sine   = F[1];

		ADC_irq_unlock();
		taskEXIT_CRITICAL();

		*lval = m_atan2f(f_sine, f_cosine) * (180.f / M_PI_F);
        }
}

static void
reg_proc_setpoint_F(const reg_t *reg, float *lval, const float *rval)
{
	float			*F = (void *) reg->link;
	float			angle, f_cosine, f_sine;
	int			revol;

        if (lval != NULL) {

		taskENTER_CRITICAL();
		ADC_irq_lock();

		f_cosine = F[0];
		f_sine   = F[1];
		revol    = pm.x_setpoint_revol;

		ADC_irq_unlock();
		taskEXIT_CRITICAL();

		angle = m_atan2f(f_sine, f_cosine);
		*lval = angle + (float) revol * 2.f * M_PI_F;
        }
        else if (rval != NULL) {

		angle = (*rval);
		revol = (int) (angle / (2.f * M_PI_F));
                angle -= (float) (revol * 2.f * M_PI_F);

                if (angle < - M_PI_F) {

                        revol -= 1;
                        angle += 2.f * M_PI_F;
                }

                if (angle > M_PI_F) {

                        revol += 1;
                        angle -= 2.f * M_PI_F;
                }

		f_cosine = m_cosf(angle);
		f_sine   = m_sinf(angle);

		taskENTER_CRITICAL();
		ADC_irq_lock();

		F[0] = f_cosine;
		F[1] = f_sine;
		pm.x_setpoint_revol = revol;

		ADC_irq_unlock();
		taskEXIT_CRITICAL();
        }
}

static void
reg_proc_setpoint_F_g(const reg_t *reg, float *lval, const float *rval)
{
	float			angle;

	if (lval != NULL) {

		reg_proc_setpoint_F(reg, &angle, rval);

		*lval = angle * (180.f / M_PI_F) / pm.const_Zp;
	}
	else if (rval != NULL) {

		angle = (*rval) * (M_PI_F / 180.f) * pm.const_Zp;

		reg_proc_setpoint_F(reg, lval, &angle);
	}
}

static void
reg_proc_setpoint_F_mm(const reg_t *reg, float *lval, const float *rval)
{
	float			angle;

	if (lval != NULL) {

		reg_proc_setpoint_F(reg, &angle, rval);

		*lval = angle * pm.const_ld_S * 1000.f
			/ (2.f * M_PI_F * pm.const_Zp);
	}
	else if (rval != NULL) {

		if (pm.const_ld_S > M_EPS_F) {

			angle = (*rval) * (2.f * M_PI_F * pm.const_Zp)
				/ (pm.const_ld_S * 1000.f);

			reg_proc_setpoint_F(reg, lval, &angle);
		}
	}
}

static void
reg_proc_km(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f / 1000.f;
	}
	else if (rval != NULL) {

		reg->link->f = (*rval) * 1000.f;
	}
}

static void
reg_proc_mm(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f * 1000.f;
	}
	else if (rval != NULL) {

		reg->link->f = (*rval) / 1000.f;
	}
}

static void
reg_proc_gain_accel(const reg_t *reg, float *lval, const float *rval)
{
        if (lval != NULL) {

                *lval = reg->link->f * reg->link->f / 2.f;
        }
        else if (rval != NULL) {

                reg->link->f = m_sqrtf((*rval) * 2.f);
        }
}

static void
reg_proc_gain_accel_mm(const reg_t *reg, float *lval, const float *rval)
{
	float			rads;

	if (lval != NULL) {

		reg_proc_gain_accel(reg, &rads, NULL);

		*lval = rads * pm.const_ld_S * 1000.f
			/ (2.f * M_PI_F * pm.const_Zp);
	}
	else if (rval != NULL) {

		if (pm.const_ld_S > M_EPS_F) {

			rads = (*rval) * (2.f * M_PI_F * pm.const_Zp)
				/ (pm.const_ld_S * 1000.f);

			reg_proc_gain_accel(reg, NULL, &rads);
		}
	}
}

static void
reg_proc_tvm_FIR_tau(const reg_t *reg, float *lval, const float *rval)
{
	float		*FIR = (void *) reg->link;
	float		tau;

	if (lval != NULL) {

		tau = FIR[0] / - FIR[1];
		tau = (tau > M_EPS_F) ? pm.dT * 1000000.f / m_logf(tau) : 0.f;

		*lval = tau;
	}
}

static void
reg_proc_hall_ST_g(const reg_t *reg, float *lval, const float *rval)
{
	float		*F = (void *) reg->link;

	if (lval != NULL) {

                *lval = m_atan2f(F[1], F[0]) * (180.f / M_PI_F);
        }
}

static void
reg_proc_im_fuel(const reg_t *reg, float *lval, const float *rval)
{
	if (lval != NULL) {

		*lval = reg->link->f;
	}
	else if (rval != NULL) {

		if (*rval < M_EPS_F) {

			taskENTER_CRITICAL();
			ADC_irq_lock();

			pm.im_consumed_Wh = 0.f;
			pm.im_consumed_Ah = 0.f;
			pm.im_reverted_Wh = 0.f;
			pm.im_reverted_Ah = 0.f;

			ADC_irq_unlock();
			taskEXIT_CRITICAL();

			vTaskDelay((TickType_t) 1);
		}
	}
}

static void
reg_format_self_BST(const reg_t *reg)
{
	int		*BST = (void *) reg->link;

	printf("%4f %4f %4f (s)", &BST[0], &BST[1], &BST[2]);
}

static void
reg_format_self_BM(const reg_t *reg)
{
	int		*BM = (void *) reg->link;

	printf("%2x %2x %2x %2x %2x %2x %2x", BM[0], BM[1], BM[2], BM[3], BM[4], BM[5], BM[6]);
}

static void
reg_format_self_RMSi(const reg_t *reg)
{
	float		*RMS = (void *) reg->link;

	printf("%3f %3f (A) %4f (V)", &RMS[0], &RMS[1], &RMS[2]);
}

static void
reg_format_self_RMSu(const reg_t *reg)
{
	float		*RMS = (void *) reg->link;

	printf("%4f %4f %4f (V)", &RMS[0], &RMS[1], &RMS[2]);
}

#define TEXT_ITEM(t)	case t: printf("(%s)", PM_SFI(t)); break

static void
reg_format_enum(const reg_t *reg)
{
	int			n, val;

	n = (int) (reg - regfile);
	val = reg->link->i;

	printf("%i ", val);

	switch (n) {

		case ID_HAL_TIM_MODE:

			switch (val) {

				TEXT_ITEM(TIM_DISABLED);
				TEXT_ITEM(TIM_DRIVE_HALL);
				TEXT_ITEM(TIM_DRIVE_QENC);

				default: break;
			}
			break;

		case ID_HAL_PPM_MODE:

			switch (val) {

				TEXT_ITEM(PPM_DISABLED);
				TEXT_ITEM(PPM_PULSE_WIDTH);
				TEXT_ITEM(PPM_STEP_DIR);
				TEXT_ITEM(PPM_CONTROL_QENC);

				default: break;
			}
			break;

		case ID_PM_FAIL_REASON:

			printf("(%s)", pm_strerror(pm.fail_reason));
			break;

		case ID_PM_CONFIG_NOP:

			switch (val) {

				TEXT_ITEM(PM_NOP_THREE_PHASE);
				TEXT_ITEM(PM_NOP_TWO_PHASE);

				default: break;
			}
			break;

		case ID_PM_CONFIG_CURRENT:

			switch (val) {

				TEXT_ITEM(PM_CURRENT_AB_INLINE);
				TEXT_ITEM(PM_CURRENT_AB_LOW);
				TEXT_ITEM(PM_CURRENT_FULL_LOW);

				default: break;
			}
			break;

		case ID_PM_CONFIG_TVM:
		case ID_PM_CONFIG_VSI_SILENT:
		case ID_PM_CONFIG_FORCED:
		case ID_PM_CONFIG_QENC_FORCED_ALIGN:
		case ID_PM_CONFIG_HFI:
		case ID_PM_CONFIG_WEAK:
		case ID_PM_CONFIG_SERVO:
		case ID_PM_CONFIG_INFO:
		case ID_PM_TVM_READY:
		case ID_PM_HALL_READY:

			switch (val) {

				TEXT_ITEM(PM_DISABLED);
				TEXT_ITEM(PM_ENABLED);

				default: break;
			}
			break;

		case ID_PM_CONFIG_ESTIMATE:

			switch (val) {

				TEXT_ITEM(PM_ESTIMATE_DISABLED);
				TEXT_ITEM(PM_ESTIMATE_FLUX);
				TEXT_ITEM(PM_ESTIMATE_KALMAN);

				default: break;
			}
			break;

		case ID_PM_CONFIG_SENSOR:

			switch (val) {

				TEXT_ITEM(PM_SENSOR_DISABLED);
				TEXT_ITEM(PM_SENSOR_HALL);
				TEXT_ITEM(PM_SENSOR_QENC);

				default: break;
			}
			break;

		case ID_PM_CONFIG_DRIVE:

			switch (val) {

				TEXT_ITEM(PM_DRIVE_CURRENT);
				TEXT_ITEM(PM_DRIVE_SPEED);

				default: break;
			}
			break;

		case ID_PM_FSM_REQ:
		case ID_PM_FSM_STATE:

			switch (val) {

				TEXT_ITEM(PM_STATE_IDLE);
				TEXT_ITEM(PM_STATE_ZERO_DRIFT);
				TEXT_ITEM(PM_STATE_SELF_TEST_BOOTSTRAP);
				TEXT_ITEM(PM_STATE_SELF_TEST_POWER_STAGE);
				TEXT_ITEM(PM_STATE_SELF_TEST_CLEARANCE);
				TEXT_ITEM(PM_STATE_ADJUST_VOLTAGE);
				TEXT_ITEM(PM_STATE_ADJUST_CURRENT);
				TEXT_ITEM(PM_STATE_PROBE_CONST_R);
				TEXT_ITEM(PM_STATE_PROBE_CONST_L);
				TEXT_ITEM(PM_STATE_LU_STARTUP);
				TEXT_ITEM(PM_STATE_LU_SHUTDOWN);
				TEXT_ITEM(PM_STATE_PROBE_CONST_E);
				TEXT_ITEM(PM_STATE_PROBE_CONST_J);
				TEXT_ITEM(PM_STATE_PROBE_LU_MPPE);
				TEXT_ITEM(PM_STATE_ADJUST_HALL);
				TEXT_ITEM(PM_STATE_HALT);

				default: break;
			}
			break;

		case ID_PM_LU_MODE:

			switch (val) {

				TEXT_ITEM(PM_LU_DISABLED);
				TEXT_ITEM(PM_LU_DETACHED);
				TEXT_ITEM(PM_LU_FORCED);
				TEXT_ITEM(PM_LU_ESTIMATE_FLUX);
				TEXT_ITEM(PM_LU_ESTIMATE_HFI);
				TEXT_ITEM(PM_LU_SENSOR_HALL);
				TEXT_ITEM(PM_LU_SENSOR_QENC);

				default: break;
			}
			break;

		case ID_PM_LU_FLUX_ZONE:

			switch (val) {

				TEXT_ITEM(PM_FLUX_UNCERTAIN);
				TEXT_ITEM(PM_FLUX_HIGH);
				TEXT_ITEM(PM_FLUX_DETACHED);

				default: break;
			}
			break;

		default: break;
	}
}

const reg_t		regfile[] = {

	REG_DEF(null,,				"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(silent,,			"",	"%i",	0, &reg_proc_silent, NULL),

	REG_DEF(hal.HSE_crystal_clock,,		"Hz",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(hal.USART_baud_rate,,		"",	"%i",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.PWM_frequency,,		"Hz",	"%1f",	REG_CONFIG, &reg_proc_pwm, NULL),
	REG_DEF(hal.PWM_deadtime,,		"ns",	"%1f",	REG_CONFIG, &reg_proc_pwm, NULL),
	REG_DEF(hal.ADC_reference_voltage,,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.ADC_shunt_resistance,,	"Ohm",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.ADC_amplifier_gain,,	"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.ADC_voltage_ratio,,		"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.ADC_terminal_ratio,,	"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.ADC_terminal_bias,,		"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.TIM_mode,,		"",	"%i", REG_CONFIG, &reg_proc_tim, &reg_format_enum),
	REG_DEF(hal.PPM_mode,,		"",	"%i", REG_CONFIG, &reg_proc_ppm, &reg_format_enum),
	REG_DEF(hal.PPM_timebase,,		"Hz",	"%i",	REG_CONFIG, NULL, NULL),
	REG_DEF(hal.PPM_signal_caught,,		"",	"%i",	REG_READ_ONLY, NULL, NULL),

	REG_DEF(ap.ppm_reg_ID,,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ap.ppm_pulse_range[0],,		"us",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_pulse_range[1],,		"us",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_pulse_range[2],,		"us",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_pulse_lost[0],,		"us",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_pulse_lost[1],,		"us",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_control_range[0],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_control_range[1],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_control_range[2],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_startup_range[0],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ppm_startup_range[1],,	"",	"%2f",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.step_reg_ID,,		"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ap.step_accuEP,,		"",	"%i",	0, NULL, NULL),
	REG_DEF(ap.step_const_ld_EP,,		"mm",	"%3f",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.analog_enabled,,		"",	"%i",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_reg_ID,,		"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ap.analog_voltage_ratio,,	"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_timeout,,		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_ANALOG[0],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_ANALOG[1],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_ANALOG[2],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_BRAKE[0],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_BRAKE[1],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_BRAKE[2],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_lost[0],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_voltage_lost[1],,	"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_control_ANALOG[0],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_control_ANALOG[1],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_control_ANALOG[2],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_control_BRAKE[0],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_control_BRAKE[1],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_control_BRAKE[2],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_startup_range[0],,	"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.analog_startup_range[1],,	"",	"%2f",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.ntc_PCB.r_balance,,		"Ohm",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_PCB.r_ntc_0,,		"Ohm",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_PCB.ta_0,,		"C",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_PCB.betta,,		"",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_EXT.r_balance,,		"Ohm",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_EXT.r_ntc_0,,		"Ohm",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_EXT.ta_0,,		"C",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.ntc_EXT.betta,,		"",	"%1f",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.temp_PCB,,			"C",	"%1f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(ap.temp_EXT,,			"C",	"%1f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(ap.temp_INT,,			"C",	"%1f",	REG_READ_ONLY, NULL, NULL),

	REG_DEF(ap.heat_PCB,,			"C",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.heat_PCB_derated_i,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.heat_EXT,,			"C",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.heat_EXT_derated_i,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.heat_PCB_FAN,,		"C",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.heat_gap,,			"C",	"%1f",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.pull_g,,			"g",	"%1f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(ap.pull_ad[0],,			"g",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.pull_ad[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.servo_span_mm[0],,		"mm",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.servo_span_mm[1],,		"mm",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.servo_uniform_mmps,,		"mm/s",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(ap.servo_mice_role,,		"",	"%i",	REG_CONFIG, NULL, NULL),

	REG_DEF(ap.FT_grab_hz,,			"Hz",	"%i",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.dc_resolution,,	"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.dc_minimal,,		"us",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.dc_clearance,,	"us",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.dc_bootstrap,,	"s",	"%4f",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.fail_reason,,	"",	"%i",	REG_READ_ONLY, NULL, &reg_format_enum),
	REG_DEF(pm.self_BST,,		"",	"%i",	REG_READ_ONLY, NULL, &reg_format_self_BST),
	REG_DEF(pm.self_BM,,		"",	"%i",	REG_READ_ONLY, NULL, &reg_format_self_BM),
	REG_DEF(pm.self_RMSi,,		"",	"%i",	REG_READ_ONLY, NULL, &reg_format_self_RMSi),
	REG_DEF(pm.self_RMSu,,		"",	"%i",	REG_READ_ONLY, NULL, &reg_format_self_RMSu),

	REG_DEF(pm.config_NOP,,		"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_TVM,,		"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_CURRENT,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_VSI_SILENT,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_FORCED,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_QENC_FORCED_ALIGN,,"","%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_ESTIMATE,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_HFI,,		"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_SENSOR,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_WEAK,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_DRIVE,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_SERVO,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_INFO,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),
	REG_DEF(pm.config_BOOST,,	"",	"%i",	REG_CONFIG, NULL, &reg_format_enum),

	REG_DEF(pm.fsm_req,,		"",	"%i",	0, NULL, &reg_format_enum),
	REG_DEF(pm.fsm_state,,		"",	"%i",	REG_READ_ONLY, NULL, &reg_format_enum),
	REG_DEF(pm.fsm_phase,,		"",	"%i",	REG_READ_ONLY, NULL, NULL),

	REG_DEF(pm.tm_transient_slow,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_transient_fast,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_voltage_hold,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_current_hold,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_instant_probe,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_average_drift,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_average_probe,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tm_startup,,			"s",	"%4f",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.ad_IA[0],,			"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_IA[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_IB[0],,			"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_IB[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_US[0],,			"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_US[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_UA[0],,			"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_UA[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_UB[0],,			"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_UB[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_UC[0],,			"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.ad_UC[1],,			"",	"%4e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.fb_iA,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.fb_iB,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.fb_uA,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.fb_uB,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.fb_uC,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.fb_HS,,			"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.fb_EP,,			"",	"%i",	REG_READ_ONLY, NULL, NULL),

	REG_DEF(pm.probe_current_hold,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_current_bias_Q,,	"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_current_sine,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_freq_sine_hz,,		"Hz",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_speed_hold,,		"rad/s","%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_speed_hold, _rpm,	"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.probe_speed_hold, _mmps,	"mm/s",	"%2f",	0, &reg_proc_mmps, NULL),
	REG_DEF(pm.probe_speed_spinup,,		"rad/s","%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_speed_spinup, _rpm,	"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.probe_speed_spinup, _mmps,	"mm/s",	"%2f",	0, &reg_proc_mmps, NULL),
	REG_DEF(pm.probe_speed_detached,,	"rad/s","%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_speed_detached, _rpm,	"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.probe_speed_detached, _mmps,	"mm/s",	"%2f",	0, &reg_proc_mmps, NULL),
	REG_DEF(pm.probe_gain_P,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.probe_gain_I,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.fault_voltage_tol,,		"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.fault_current_tol,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.fault_accuracy_tol,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.fault_current_halt,,		"A",	"%3f",	REG_CONFIG, &reg_proc_halt, NULL),
	REG_DEF(pm.fault_voltage_halt,,		"V",	"%3f",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.vsi_DC,,			"",	"%4f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.vsi_X,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.vsi_Y,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.vsi_DX,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.vsi_DY,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.vsi_IF,,			"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.vsi_UF,,			"",	"%i",	REG_READ_ONLY, NULL, NULL),

	REG_DEF(pm.tvm_READY,,	"",	"%i",	REG_CONFIG | REG_READ_ONLY, NULL, &reg_format_enum),
	REG_DEF(pm.tvm_range_DC,,		"",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.tvm_A,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_B,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_C,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_A[0],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_A[1],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_A[2],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_A, _tau,	"us",	"%3f",	REG_READ_ONLY, &reg_proc_tvm_FIR_tau, NULL),
	REG_DEF(pm.tvm_FIR_B[0],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_B[1],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_B[2],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_B, _tau,	"us",	"%3f",	REG_READ_ONLY, &reg_proc_tvm_FIR_tau, NULL),
	REG_DEF(pm.tvm_FIR_C[0],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_C[1],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_C[2],,	"",	"%4e",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_FIR_C, _tau,	"us",	"%3f",	REG_READ_ONLY, &reg_proc_tvm_FIR_tau, NULL),
	REG_DEF(pm.tvm_DX,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.tvm_DY,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),

	REG_DEF(pm.lu_iX,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_iY,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_iD,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_iQ,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_F[0],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_F[1],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_F, _g,			"g",	"%2f",	REG_READ_ONLY, &reg_proc_F_g, NULL),
	REG_DEF(pm.lu_wS,,		"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_wS, _rpm,			"rpm",	"%2f",	REG_READ_ONLY, &reg_proc_rpm, NULL),
	REG_DEF(pm.lu_wS, _mmps,		"mm/s",	"%2f",	REG_READ_ONLY, &reg_proc_mmps, NULL),
	REG_DEF(pm.lu_wS, _kmh,			"km/h",	"%1f",	REG_READ_ONLY, &reg_proc_kmh, NULL),
	REG_DEF(pm.lu_mode,,			"",	"%i",	REG_READ_ONLY, NULL, &reg_format_enum),

	REG_DEF(pm.lu_flux_lpf_wS,,	"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.lu_flux_lpf_wS, _rpm,	"rpm",	"%2f",	REG_READ_ONLY, &reg_proc_rpm, NULL),
	REG_DEF(pm.lu_flux_lpf_wS, _mmps,	"mm/s",	"%2f",	REG_READ_ONLY, &reg_proc_mmps, NULL),
	REG_DEF(pm.lu_flux_lpf_wS, _kmh,	"km/h",	"%1f",	REG_READ_ONLY, &reg_proc_kmh, NULL),
	REG_DEF(pm.lu_MPPE,,		"rad/s",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.lu_MPPE, _rpm,		"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.lu_flux_zone,,		"",	"%i",	REG_READ_ONLY, NULL, &reg_format_enum),
	REG_DEF(pm.lu_gain_TAKE,,		"",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.lu_gain_GIVE,,		"",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.lu_gain_TAKE, _BEMF,		"V",	"%3f",	0, &reg_proc_BEMF, NULL),
	REG_DEF(pm.lu_gain_GIVE, _BEMF,		"V",	"%3f",	0, &reg_proc_BEMF, NULL),
	REG_DEF(pm.lu_gain_LEVE,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.lu_gain_LP,,			"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.forced_hold_D,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.forced_maximal,,	"rad/s",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.forced_maximal, _rpm,	"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.forced_reverse,,	"rad/s",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.forced_reverse, _rpm,	"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.forced_accel,,	"rad/s2",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.forced_accel, _rpm,	"rpm/s",	"%1f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.forced_accel, _mmps,	"mm/s2",	"%2f",	0, &reg_proc_mmps, NULL),

	REG_DEF(pm.detach_take_U,,		"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.detach_gain_AD,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.detach_gain_SF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.flux_X,,			"Wb",	"%4e",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.flux_Y,,			"Wb",	"%4e",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.flux_E,,			"Wb",	"%4e",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.flux_F[0],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.flux_F[1],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.flux_F, _g,			"g",	"%2f",	REG_READ_ONLY, &reg_proc_F_g, NULL),
	REG_DEF(pm.flux_wS,,		"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.flux_wS, _rpm,		"rpm",	"%2f",	REG_READ_ONLY, &reg_proc_rpm, NULL),
	REG_DEF(pm.flux_wS, _mmps,		"mm/s",	"%2f",	REG_READ_ONLY, &reg_proc_mmps, NULL),
	REG_DEF(pm.flux_wS, _kmh,		"km/h",	"%1f",	REG_READ_ONLY, &reg_proc_kmh, NULL),
	REG_DEF(pm.flux_gain_IN,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.flux_gain_LO,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.flux_gain_HI,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.flux_gain_AD,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.flux_gain_SF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.hfi_freq_hz,,		"Hz",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hfi_swing_D,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hfi_F[0],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hfi_F[1],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hfi_F, _g,			"g",	"%2f",	REG_READ_ONLY, &reg_proc_F_g, NULL),
	REG_DEF(pm.hfi_wS,,		"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hfi_wS, _rpm,		"rpm",	"%2f",	REG_READ_ONLY, &reg_proc_rpm, NULL),
	REG_DEF(pm.hfi_wS, _mmps,		"mm/s",	"%2f",	REG_READ_ONLY, &reg_proc_mmps, NULL),
	REG_DEF(pm.hfi_wS, _kmh,		"km/h",	"%1f",	REG_READ_ONLY, &reg_proc_kmh, NULL),
	REG_DEF(pm.hfi_polarity,,		"",	"%4e",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hfi_gain_EP,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hfi_gain_SF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hfi_gain_FP,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.hall_ST[1].X,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[1].Y,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[1], _g,	"g",	"%2f",	REG_READ_ONLY, &reg_proc_hall_ST_g, NULL),
	REG_DEF(pm.hall_ST[2].X,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[2].Y,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[2], _g,	"g",	"%2f",	REG_READ_ONLY, &reg_proc_hall_ST_g, NULL),
	REG_DEF(pm.hall_ST[3].X,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[3].Y,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[3], _g,	"g",	"%2f",	REG_READ_ONLY, &reg_proc_hall_ST_g, NULL),
	REG_DEF(pm.hall_ST[4].X,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[4].Y,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[4], _g,	"g",	"%2f",	REG_READ_ONLY, &reg_proc_hall_ST_g, NULL),
	REG_DEF(pm.hall_ST[5].X,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[5].Y,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[5], _g,	"g",	"%2f",	REG_READ_ONLY, &reg_proc_hall_ST_g, NULL),
	REG_DEF(pm.hall_ST[6].X,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[6].Y,,	"",	"%3f",	REG_CONFIG | REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_ST[6], _g,	"g",	"%2f",	REG_READ_ONLY, &reg_proc_hall_ST_g, NULL),
	REG_DEF(pm.hall_READY,,	"",	"%i",	REG_CONFIG | REG_READ_ONLY, NULL, &reg_format_enum),
	REG_DEF(pm.hall_DIRF,,			"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_prolS,,			"rad",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_prolTIM,,		"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_F[0],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_F[1],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_F, _g,			"g",	"%2f",	REG_READ_ONLY, &reg_proc_F_g, NULL),
	REG_DEF(pm.hall_wS,,		"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_wS, _rpm,		"rpm",	"%2f",	REG_READ_ONLY, &reg_proc_rpm, NULL),
	REG_DEF(pm.hall_wS, _mmps,		"mm/s",	"%2f",	REG_READ_ONLY, &reg_proc_mmps, NULL),
	REG_DEF(pm.hall_wS, _kmh,		"km/h",	"%1f",	REG_READ_ONLY, &reg_proc_kmh, NULL),
	REG_DEF(pm.hall_lpf_wS,,	"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.hall_prol_T,, 		"s",	"%4f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hall_gain_PF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hall_gain_SF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.hall_gain_LP,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.qenc_baseEP,,		"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_lastEP,,		"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_rotEP,,			"",	"%i",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_prolS,,			"rad",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_PPR,,			"",	"%i",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.qenc_Zq,,			"",	"%5f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.qenc_F[0],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_F[1],,			"",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_F, _g,			"g",	"%2f",	REG_READ_ONLY, &reg_proc_F_g, NULL),
	REG_DEF(pm.qenc_wS,,		"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_wS, _rpm,		"rpm",	"%2f",	REG_READ_ONLY, &reg_proc_rpm, NULL),
	REG_DEF(pm.qenc_wS, _mmps,		"mm/s",	"%2f",	REG_READ_ONLY, &reg_proc_mmps, NULL),
	REG_DEF(pm.qenc_wS, _kmh,		"km/h",	"%1f",	REG_READ_ONLY, &reg_proc_kmh, NULL),
	REG_DEF(pm.qenc_lpf_wS,,	"rad/s",	"%2f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.qenc_gain_PF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.qenc_gain_SF,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.qenc_gain_LP,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.const_fb_U,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.const_E,,			"Wb",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_E, _kv,	"rpm/v",	"%2f",	0, &reg_proc_kv, NULL),
	REG_DEF(pm.const_R,,			"Ohm",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_L,,			"H",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_Zp,,			"",	"%i",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_Ja,,		"A/rad/s2",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_Ja, _kgm2,		"kgm2",	"%4e",	0, &reg_proc_kgm2, NULL),
	REG_DEF(pm.const_Ja, _kg,		"kg",	"%4e",	0, &reg_proc_kg, NULL),
	REG_DEF(pm.const_im_LD,,		"H",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_im_LQ,,		"H",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_im_B,,			"g",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_im_R,,			"Ohm",	"%4e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.const_ld_S,,			"mm",	"%3f",	REG_CONFIG, &reg_proc_mm, NULL),

	REG_DEF(pm.watt_wP_maximal,,		"W",	"%1f",	REG_CONFIG, &reg_proc_watt, NULL),
	REG_DEF(pm.watt_iDC_maximal,,		"A",	"%3f",	REG_CONFIG, &reg_proc_watt, NULL),
	REG_DEF(pm.watt_wP_reverse,,		"W",	"%1f",	REG_CONFIG, &reg_proc_watt, NULL),
	REG_DEF(pm.watt_iDC_reverse,,		"A",	"%3f",	REG_CONFIG, &reg_proc_watt, NULL),
	REG_DEF(pm.watt_dclink_HI,,		"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.watt_dclink_LO,,		"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.watt_lpf_D,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.watt_lpf_Q,,			"V",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.watt_lpf_wP,,		"W",	"%1f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.watt_gain_LP_F,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.watt_gain_LP_P,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.i_maximal,,			"A",	"%3f",	REG_CONFIG, &reg_proc_maximal_i, NULL),
	REG_DEF(pm.i_reverse,,			"A",	"%3f",	REG_CONFIG, &reg_proc_reverse_i, NULL),
	REG_DEF(pm.i_derated_1,,		"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.i_derated_HFI,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.i_setpoint_D,,		"A",	"%3f",	0, NULL, NULL),
	REG_DEF(pm.i_setpoint_Q,,		"A",	"%3f",	0, NULL, NULL),
	REG_DEF(pm.i_setpoint_Q, _pc,		"pc",	"%2f",	0, &reg_proc_Q_pc, NULL),
	REG_DEF(pm.i_gain_P,,			"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.i_gain_I,,			"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.inject_ratio_D,,		"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.inject_gain_AD,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.weak_maximal,,		"A",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.weak_bias_U,,		"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.weak_D,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.weak_gain_EU,,		"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.v_maximal,,			"V",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.v_reverse,,			"V",	"%3f",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.s_maximal,,		"rad/s",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.s_maximal, _rpm,		"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.s_maximal, _kmh,		"km/h",	"%1f",	0, &reg_proc_kmh, NULL),
	REG_DEF(pm.s_reverse,,		"rad/s",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.s_reverse, _rpm,		"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.s_reverse, _kmh,		"km/h",	"%1f",	0, &reg_proc_kmh, NULL),
	REG_DEF(pm.s_setpoint,,		"rad/s",	"%2f",	0, NULL, NULL),
	REG_DEF(pm.s_setpoint, _rpm,		"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.s_setpoint, _mmps,		"mm/s",	"%2f",	0, &reg_proc_mmps, NULL),
	REG_DEF(pm.s_setpoint, _kmh,		"km/h",	"%1f",	0, &reg_proc_kmh, NULL),
	REG_DEF(pm.s_setpoint, _pc,		"pc",	"%2f",	0, &reg_proc_rpm_pc, NULL),
	REG_DEF(pm.s_accel,,		"rad/s2",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.s_accel, _rpm,	"rpm/s",	"%1f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.s_accel, _kmh,	"km/h/s",	"%1f",	0, &reg_proc_kmh, NULL),
	REG_DEF(pm.s_integral,,			"A",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.s_gain_P,,			"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.s_gain_I,,			"",	"%2e",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.s_gain_S,,			"",	"%2e",	REG_CONFIG, NULL, NULL),

	REG_DEF(pm.x_setpoint_F,,		"rad",	"%2f",	0, &reg_proc_setpoint_F, NULL),
	REG_DEF(pm.x_setpoint_F, _g,		"g",	"%2f",	0, &reg_proc_setpoint_F_g, NULL),
	REG_DEF(pm.x_setpoint_F, _mm,		"mm",	"%3f",	0, &reg_proc_setpoint_F_mm, NULL),
	REG_DEF(pm.x_setpoint_wS,,	"rad/s",	"%2f",	0, NULL, NULL),
	REG_DEF(pm.x_setpoint_wS, _rpm,		"rpm",	"%2f",	0, &reg_proc_rpm, NULL),
	REG_DEF(pm.x_setpoint_wS, _mmps,	"mm/s",	"%2f",	0, &reg_proc_mmps, NULL),
	REG_DEF(pm.x_near_tol,,			"rad",	"%2f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.x_near_tol, _mm,		"mm",	"%3f",	0, &reg_proc_mmps, NULL),
	REG_DEF(pm.x_gain_P,,			"",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.x_gain_P, _accel,	"rad/s2",	"%1f",	0, &reg_proc_gain_accel, NULL),
	REG_DEF(pm.x_gain_P, _accel_mm,	"mm/s2",	"%1f",	0, &reg_proc_gain_accel_mm, NULL),
	REG_DEF(pm.x_gain_N,,			"",	"%1f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.x_gain_N, _accel,	"rad/s2",	"%1f",	0, &reg_proc_gain_accel, NULL),
	REG_DEF(pm.x_gain_N, _accel_mm,	"mm/s2",	"%1f",	0, &reg_proc_gain_accel_mm, NULL),

	REG_DEF(pm.im_revol_total,,		"",	"%i",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.im_distance,,		"m",	"%1f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.im_distance, _km,		"km",	"%3f",	REG_READ_ONLY, &reg_proc_km, NULL),
	REG_DEF(pm.im_consumed_Wh,,		"Wh",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.im_consumed_Ah,,		"Ah",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.im_reverted_Wh,,		"Wh",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.im_reverted_Ah,,		"Ah",	"%3f",	REG_READ_ONLY, NULL, NULL),
	REG_DEF(pm.im_capacity_Ah,,		"Ah",	"%3f",	REG_CONFIG, NULL, NULL),
	REG_DEF(pm.im_fuel_pc,,			"pc",	"%2f",	0, &reg_proc_im_fuel, NULL),

	REG_DEF(ti.reg_ID[0],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[1],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[2],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[3],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[4],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[5],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[6],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[7],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[8],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),
	REG_DEF(ti.reg_ID[9],,			"",	"%i",	REG_CONFIG | REG_LINKED, NULL, NULL),

	{ NULL, "", 0, NULL, NULL, NULL }
};

void reg_getval(const reg_t *reg, void *lval)
{
	if (reg->proc != NULL) {

		reg->proc(reg, lval, NULL);
	}
	else {
		*(reg_val_t *) lval = *reg->link;
	}
}

void reg_setval(const reg_t *reg, const void *rval)
{
	if ((reg->mode & REG_READ_ONLY) == 0) {

		if (reg->proc != NULL) {

			reg->proc(reg, NULL, rval);
		}
		else {
			*reg->link = *(reg_val_t *) rval;
		}
	}
}

void reg_format_rval(const reg_t *reg, const void *rval)
{
	reg_val_t		*link = (reg_val_t *) rval;

	if (reg->fmt[1] == 'i') {

		printf(reg->fmt, link->i);
	}
	else {
		printf(reg->fmt, &link->f);
	}
}

void reg_format(const reg_t *reg)
{
	reg_val_t		rval;
	const char		*su;

	if (reg != NULL) {

		printf("%c%c%c [%i] %s = ",
			(int) (reg->mode & REG_CONFIG) 		? 'C' : ' ',
			(int) (reg->mode & REG_READ_ONLY)	? 'R' : ' ',
			(int) (reg->mode & REG_LINKED)		? 'L' : ' ',
			(int) (reg - regfile), reg->sym);

		if (reg->format != NULL) {

			reg->format(reg);
		}
		else {
			reg_getval(reg, &rval);
			reg_format_rval(reg, &rval);

			if (reg->mode & REG_LINKED) {

				if (rval.i >= 0 && rval.i < REG_MAX) {

					printf(" (%s)", regfile[rval.i].sym);
				}
			}

			su = reg->sym + strlen(reg->sym) + 1;

			if (*su != 0) {

				printf(" (%s)", su);
			}
		}

		puts(EOL);
	}
}

const reg_t *reg_search(const char *sym)
{
	const reg_t		*reg, *found = NULL;
	int			n;

	if (stoi(&n, sym) != NULL) {

		if (n >= 0 && n < REG_MAX)
			found = regfile + n;
	}
	else {
		for (reg = regfile; reg->sym != NULL; ++reg) {

			if (strcmp(reg->sym, sym) == 0) {

				found = reg;
				break;
			}
		}

		if (found == NULL && silent == NULL) {

			for (reg = regfile; reg->sym != NULL; ++reg) {

				if (strstr(reg->sym, sym) != NULL) {

					if (found == NULL) {

						found = reg;
					}
					else {
						found = NULL;
						break;
					}
				}
			}
		}
	}

	return found;
}

void reg_GET(int n, void *lval)
{
	if (n >= 0 && n < REG_MAX) {

		reg_getval(regfile + n, lval);
	}
}

void reg_SET(int n, const void *rval)
{
	if (n >= 0 && n < REG_MAX) {

		reg_setval(regfile + n, rval);
	}
}

int reg_GET_I(int n)
{
	int		lval;

	reg_GET(n, &lval);

	return lval;
}

float reg_GET_F(int n)
{
	float		lval;

	reg_GET(n, &lval);

	return lval;
}

void reg_SET_I(int n, int rval)
{
	reg_SET(n, &rval);
}

void reg_SET_F(int n, float rval)
{
	reg_SET(n, &rval);
}

SH_DEF(reg)
{
	reg_val_t		rval;
	const reg_t		*reg, *lreg;

	reg = reg_search(s);

	if (reg != NULL) {

		s = sh_next_arg(s);

		if (reg->fmt[1] == 'i') {

			if (reg->mode & REG_LINKED) {

				lreg = reg_search(s);

				if (lreg != NULL) {

					rval.i = (int) (lreg - regfile);
					reg_setval(reg, &rval);
				}
			}
			else if (stoi(&rval.i, s) != NULL) {

				reg_setval(reg, &rval);
			}
		}
		else {
			if (stof(&rval.f, s) != NULL) {

				reg_setval(reg, &rval);
			}
		}

		reg_format(reg);
	}
	else {
		for (reg = regfile; reg->sym != NULL; ++reg) {

			if (strstr(reg->sym, s) != NULL) {

				reg_format(reg);
			}
		}
	}
}

SH_DEF(conf_export)
{
	reg_val_t		rval;
	const reg_t		*reg;

	puts("reg silent 1" EOL);

	for (reg = regfile; reg->sym != NULL; ++reg) {

		if (reg->mode & REG_CONFIG) {

			printf("reg %s ", reg->sym);

			reg_getval(reg, &rval);

			if (reg->mode & REG_LINKED) {

				if (rval.i >= 0 && rval.i < REG_MAX) {

					puts(regfile[rval.i].sym);
				}
			}
			else {
				reg_format_rval(reg, &rval);
			}

			puts(EOL);
		}
	}

	puts("reg silent 0" EOL);
}

