#define HW_MCU_STM32F405

#define HW_HAVE_ANALOG_KNOB
#define HW_HAVE_NTC_ON_PCB
#define HW_HAVE_NTC_MOTOR
#define HW_HAVE_NETWORK_CAN

#define HW_CLOCK_CRYSTAL_HZ		12000000UL

#define HW_PWM_FREQUENCY_HZ		30000.f
#define HW_PWM_DEADTIME_NS		170.f		/* IPT007N06N */

#define HW_PWM_MINIMAL_PULSE		0.2f
#define HW_PWM_CLEARANCE_ZONE		5.0f
#define HW_PWM_SKIP_ZONE		2.0f
#define HW_PWM_BOOTSTRAP_RETENTION	90.f		/* UCC27211A  */

#define HW_ADC_SAMPLING_SCHEME		ADC_SEQUENCE__ABU_TTT

#define HW_ADC_REFERENCE_VOLTAGE	3.3f
#define HW_ADC_SHUNT_RESISTANCE		0.0005f
#define HW_ADC_AMPLIFIER_GAIN		20.f		/* AD8418 */

#define HW_ADC_VOLTAGE_R1		470000.f
#define HW_ADC_VOLTAGE_R2		27000.f
#define HW_ADC_VOLTAGE_BIAS_R3		470000.f

#define HW_ADC_KNOB_R1			10000.f
#define HW_ADC_KNOB_R2			10000.f

#define HW_NTC_PCB_TYPE			NTC_GND
#define HW_NTC_PCB_BALANCE		10000.f
#define HW_NTC_PCB_NTC_0		10000.f
#define HW_NTC_PCB_TA_0			25.f
#define HW_NTC_PCB_BETTA		3435.f		/* EWTF05-103H3I */

#define GPIO_ADC_CURRENT_A		XGPIO_DEF3('A', 3, 3)
#define GPIO_ADC_CURRENT_B		XGPIO_DEF3('A', 2, 2)
#define GPIO_ADC_VOLTAGE_U		XGPIO_DEF3('A', 1, 1)
#define GPIO_ADC_VOLTAGE_A		XGPIO_DEF3('C', 2, 12)
#define GPIO_ADC_VOLTAGE_B		XGPIO_DEF3('C', 1, 11)
#define GPIO_ADC_VOLTAGE_C		XGPIO_DEF3('C', 0, 10)
#define GPIO_ADC_NTC_PCB		XGPIO_DEF3('C', 3, 13)
#define GPIO_ADC_NTC_EXT		XGPIO_DEF3('A', 0, 0)
#define GPIO_ADC_KNOB_ANG		XGPIO_DEF3('B', 1, 9)
#define GPIO_ADC_KNOB_BRK		XGPIO_DEF3('C', 4, 14)

#define GPIO_USART3_TX			XGPIO_DEF4('C', 10, 0, 7)
#define GPIO_USART3_RX			XGPIO_DEF4('C', 11, 0, 7)

#define GPIO_CAN_RX			XGPIO_DEF4('B', 8, 0, 9)
#define GPIO_CAN_TX			XGPIO_DEF4('B', 9, 0, 9)

#define GPIO_BOOST_EN			XGPIO_DEF2('B', 2)
#define GPIO_FAN			XGPIO_DEF2('B', 12)
#define GPIO_LED			XGPIO_DEF2('C', 12)
