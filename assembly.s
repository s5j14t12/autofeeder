	.syntax unified
	.cpu cortex-m4
	.thumb

	.data
		student_id: .byte 15,15,15,15,0,15,15,15		 //TODO: put your student id
		X: .word 1
		Y: .word 10
		decodeMode_num: .word 0x000009FB
		scanLimit: .word 0x00000B07
		DIN_0: .word 0x00100000
		DIN_1: .word 0x00000010

	.text
		.global GPIO_init
		.global max7219_send
		.global max7219_init

		.equ RCC_AHB2ENR, 0x4002104C
		.equ GPIOB_MODER, 0x48000400
		.equ GPIOB_OTYPER, 0x48000404
		.equ GPIOB_OSPEEDR, 0x48000408
		.equ GPIOB_PUPDR, 0x4800040C
		.equ GPIOB_ODR, 0x48000414
		.equ GPIOB_BSRR, 0x48000418


	max7219_send:
		push {r4-r11, LR}
		mov r3, #0
		mov r3, r0
		lsl r3, r3, #8
		add r3, r3, r1
		mov r1, r3
		BL shift_bits

		pop {r4-r11, LR}
		BX LR

	max7219_init:
		push {r4-r11, lr}

		// decode mode init
		ldr r0, =decodeMode_num
		ldr r1, [r0]			// r1 is using for shift(16bits)
		BL shift_bits

		// Scan Limit Register
		ldr r0, =scanLimit
		ldr r1, [r0]
		BL shift_bits

		// Display_Test
		mov r1, 0x0000000F
		lsl r1, #8
		BL shift_bits

		// Shutdown Register
		mov r1, #0x0000000C
		lsl r1, #8
		mov r0, #1
		add r1, r1, r0
		BL shift_bits

		// Intensity Register
		mov r1, #0x0000000A
		lsl r1, #8
		mov r0, #0x00000002
		add r1, r1, r0
		BL shift_bits

		ldr r11, =student_id
		mov r1, #7
		ldrb r2, [r11]
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #6
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #5
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #4
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #3
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #2
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #1
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		add r11, r11, #1
		ldrb r2, [r11]
		mov r1, #8
		lsl r1, #8
		add r1, r1, r2
		BL shift_bits

		pop {r4-r11, LR}
		BX LR

	shift_bits:
		push {r0, r1, LR}
		ldr r7, =GPIOB_BSRR
		ldr r8, =DIN_0
		ldr r8, [r8]
		ldr r9, =DIN_1
		ldr r9, [r9]
		mov r4, #16
		mov r2, #0x00008000
	shift_loops:

		// input DIN
		and r3, r1, r2
		cmp r3, #0
		ITE EQ
		streq r8, [r7]
		strne r9, [r7]
		BL Delay

		// input Clock_To_1 and shift r1
		mov r0, #0x00000040
		str r0, [r7]
		LSL r1, r1, #1
		BL Delay

		// input Clock_To_0
		mov r0, #0x00400000
		str r0, [r7]
		subs r4, #1
		bne shift_loops

		// Set CS and Clock and than go back
		mov r0, 0x00000020
		str r0, [r7]
		BL Delay
		mov r0, 0x00000040 		// set clock
		str r0, [r7]
		BL Delay
		mov r0, #0x00600000
		str r0, [r7]
		BL Delay
		pop {r0, r1, LR}
		BX LR

	Delay:
		push {r3, r4}
		ldr r3, =X
		ldr r3, [r3]
	L1:
		ldr r4, =Y
		ldr r4, [r4]
	L2:
		subs r4, #1
		bne L2
		subs r3, #1
		bne L1
		pop {r3, r4}
		BX LR


