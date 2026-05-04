/* Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com/
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the
* distribution.
*
* Neither the name of Texas Instruments Incorporated nor the names of
* its contributors may be used to endorse or promote products derived
* from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*************************************************************************************
*   File:     icss_i2c.h
*
*   Brief:   This is a common header file with all memory map configuration.   
*************************************************************************************
*/
#ifndef ICSS_I2C_H
#define ICSS_I2C_H

#define PRUICSS_MAX_INSTANCE                           (2U)

typedef enum PRUICSS_PruCores_e
{
    PRUICCSS_PRU0,
    PRUICCSS_PRU1
}PRUICSS_PruCores;

/* The Memory Size of I2C Instance */
#define ICSS_I2C_INSTANCE_SIZE                     (0x0300)
/* The Memory offset of I2C Tx Buffer */
#define ICSS_I2C_INSTANCE_TX_OFFSET                (0x0100)
/* The Memory offset of I2C Rx Buffer */
#define ICSS_I2C_INSTANCE_RX_OFFSET                (0x0200)

#define ICSS_I2C_CONFIG_MEMORY                     (0x0000)

/* The Memory region of I2C INSTANCE0 MMap */
#define ICSS_I2C_INSTANCE0_ADDR                    (ICSS_I2C_CONFIG_MEMORY + 0x100 )

/* The Memory region of I2C INSTANCE0 Tx buffer */
#define ICSS_I2C_INSTANCE0_TX_MEM                  (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_INSTANCE_TX_OFFSET)

/* The Memory region of I2C INSTANCE0 Rx buffer */
#define ICSS_I2C_INSTANCE0_RX_MEM                  (ICSS_I2C_INSTANCE0_ADDR + ICSS_I2C_INSTANCE_RX_OFFSET)

/* The Memory offset of I2C Revision Register low value */
#define ICSS_I2C_REVNB_LO_OFFSET                   (0x00)
/* The Memory offset of I2C Revision Register high value */
#define ICSS_I2C_REVNB_HI_OFFSET                   (0x04)
/* The Memory offset of I2C command value register */
#define ICSS_I2C_COMMAND_OFFSET                    (0x08)
/* The Memory offset of I2C IRQ status register */
#define ICSS_I2C_IRQSTATUS_OFFSET                  (0x28)
/* The Memory offset of I2C Fifo size register */
#define ICSS_I2C_BUF_OFFSET                        (0x94)
/* The Memory offset of I2C data count register */
#define ICSS_I2C_CNT_OFFSET                        (0x98)
/* The Memory offset of I2C configuration register */
#define ICSS_I2C_CON_OFFSET                        (0xA4)
/* The Memory offset of I2C slave address register */
#define ICSS_I2C_SA_OFFSET                         (0xAC)
/* The Memory offset of I2C input output pin value */
#define ICSS_I2C_PRU_PIN_OFFSET                    (0xD8)
/* The Memory offset of I2C clock value register */
#define ICSS_I2C_PRU_CLK_VAL_OFFSET                (0xDC)
/* The Memory offset of I2C smbus command code register */
#define ICSS_I2C_PRU_CMD_CODE_OFFSET               (0xE0)
/* The Memory offset of I2C instance id register */
#define ICSS_I2C_PRU_INST_ID_OFFSET                (0xE4)

/* The register bit for enabling i2c instance */
#define ICSS_I2C_MODULE_ENABLE_BIT                 (15)
/* The register bit for selecting master or slave */
#define ICSS_I2C_MASTER_SLAVE_MODE_BIT             (10)
/* The register bit for selecting address mode */
#define ICSS_I2C_ADDRESSING_MODE_BIT               (8)
/* The register bit for enabling i2c burst mode */
#define ICSS_I2C_SMBUS_BURST_BIT                   (5)
/* The register bit for deciding NACK recieve */
#define ICSS_I2C_RECIEVE_NACK_BIT                  (4)
/* The register bit for deciding ACK recieve */
#define ICSS_I2C_ACK_RECIEVED_BIT                  (3)
/* The register bit for selecting between read/write */
#define ICSS_I2C_READ_WRITE_BIT                    (2)
/* The register bit for enabling start bit */
#define ICSS_I2C_START_BIT                         (0)
/* The register bit for enabling stop bit */
#define ICSS_I2C_STOP_BIT                          (1)

/* The command value for resetting I2C instance */
#define ICSS_I2C_RESET_CMD                         (0x10U)
/* The command value for setting up I2C instance */
#define ICSS_I2C_SETUP_CMD                         (0x11U)
/* The command value for start Rx for I2C instance */
#define ICSS_I2C_RX_CMD                            (0x12U)
/* The command value for start Tx for I2C instance */
#define ICSS_I2C_TX_CMD                            (0x13U)
/* The command value for start quick cmd for I2C instance */
#define ICSS_SMBUS_QUICK_CMD                       (0x14U)
/* The command value for start send byte cmd for I2C instance */
#define ICSS_SMBUS_SEND_BYTE_CMD                   (0x15U)
/* The command value for start recieve byte cmd for I2C instance */
#define ICSS_SMBUS_RECEIVE_BYTE_CMD                (0x16U)
/* The command value for start write byte cmd for I2C instance */
#define ICSS_SMBUS_WRITE_BYTE_CMD                  (0x17U)
/* The command value for start read byte cmd for I2C instance */
#define ICSS_SMBUS_READ_BYTE_CMD                   (0x18U)
/* The command value for start write word cmd for I2C instance */
#define ICSS_SMBUS_WRITE_WORD_CMD                  (0x19U)
/* The command value for start read word for I2C instance */
#define ICSS_SMBUS_READ_WORD_CMD                   (0x1AU)
/* The command value for start block write for I2C instance */
#define ICSS_SMBUS_BLOCK_WRITE_CMD                 (0x1BU)
/* The command value for start block read for I2C instance */
#define ICSS_SMBUS_BLOCK_READ_CMD                  (0x1CU)
/* The command value for start read scl cmd for I2C instance */
#define ICSS_I2C_READ_SCL_CMD                      (0x1DU)
/* The command value for start reset slave cmd for I2C instance */
#define ICSS_I2C_RESET_SLAVE_CMD                   (0x1EU)
/* The command value for loopback mode */
#define ICSS_I2C_LOOPBACK_CMD                      (0x1FU)

/* The response for command successful */
#define COMMAND_SUCCESS                            (0x0500U)
/* The response for reset command failure */
#define RESET_COMMAND_FAILED                       (0x0501U)
/* The response for setup command failure */
#define SETUP_COMMAND_FAILED                       (0x0502U)
/* The response for tx command failure */
#define TX_COMMAND_FAILED                          (0x0503U)
/* The response for rx command failure */
#define RX_COMMAND_FAILED                          (0x0504U)
/* The response for if SCL value high */
#define SCL_VALUE_HIGH                             (0x0505U)
/* The response for if SCL value low */
#define SCL_VALUE_LOW                              (0x0506U)
/* The response for sucessfull reset command */
#define RESET_SLAVE_DONE                           (0x0507U)
/* The response for NACK for address transmission */
#define ADDRESS_ACKNOWLDEGE_FAILED                 (0x0508U)
/* The response for NACK for data transmission */
#define DATA_ACKNOWLDEGE_FAILED                    (0x0509U)
/* The response for wrong master slave configuration */
#define MASTER_SLAVE_MODE_FAILED                   (0x050AU)
/* The response for wrong addressing mode configuration */
#define ADDRESSING_MODE_FAILED                     (0x050BU)
/* The response for invalid command passed configuration */
#define INVALID_COMMAND                            (0x050CU)
/* The response for incorrect data count configuration */
#define INVALID_DATA_COUNT                         (0x050DU)
/* The time out for the response to come */
#define TIME_OUT_ERROR                             (0x050EU)


/* The iep counter increment value for 400KHz value */
//  1 I2C instance, time-slice 125 (625/5) cycles.
//  => bus clock time is 125*4/200e6 = 2.5 usec.
//  => bus clock speed is 1/2.5e-6 = 400 kHz
#define IEP_CMP_INCREMENT_VAL_400KHZ               (0x00000270U)  // 400 kHz
#define IEP_CMP_INCREMENT_HALF_VAL_400KHZ          (0x00000138U)

/* The iep counter increment value for 100KHz value */
//  4 I2C instances, time-slice 125 (625/5) cycles.
//  => bus clock time is 125*4*4/200e6 = 10 usec.
//  => bus clock speed is 1/10e-6 = 100 kHz
#define IEP_CMP_INCREMENT_VAL_100KHZ          (0x000009c3U)  //  100 kHz, 1x instances
#define IEP_CMP_INCREMENT_HALF_VAL_100KHZ     (0x000004e1U)  // 

//  1 I2C instance, time-slice 64 (320/5) cycles.
//  => bus clock time is 64*4/200e6 = 1.28 usec.
//  => bus clock speed is 1/1.28e-6 = 781.25 kHz
#define IEP_CMP_INCREMENT_VAL_1MHZ                  (0x00000140U)   // 781.25 kHz, 1x instance
#define IEP_CMP_INCREMENT_HALF_VAL_1MHZ             (0x000000A0U)

/* The register offset of IRQ status for host to read */
#define IRQ_COMMON_REGISTER_OFFSET                 (0x0008U)
/* The register offset of global frequency set */
#define I2C_BUS_FREQUENCY_OFFSET                   (0x000CU)

/* The frequency configuration for I2C FW */
#define ICSS_I2C_NO_FREQ                           (0x00U)
/* The frequency configuration for I2C FW @ 1MHZ */
#define ICSS_I2C_1MHZ_FREQ                         (0x01U)
/* The frequency configuration for I2C FW @ 400KHZ */
#define ICSS_I2C_400KHZ_FREQ                       (0x02U)
/* The frequency configuration for I2C FW @ 100KHZ */
#define ICSS_I2C_100KHZ_FREQ                       (0x03U)

/* The interrupt line used for host for PRU0*/
#define ICSS_I2C_INTC_PRU0_BIT_VAL                 (0x00100000U)
/* The interrupt line used for host for PRU1*/
#define ICSS_I2C_INTC_PRU1_BIT_VAL                 (0x00200000U)

#endif