/*
*  Copyright (C) 2025 Texas Instruments Incorporated
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Typedef for ABZ handle
typedef struct ABZ_Config_s *ABZ_Handle;

// Enum for different states
typedef enum
{
    // States for Prev_A_edge=0, Prev_B_edge=0
    STATE_00_00,
    STATE_00_01,
    STATE_00_10,
    STATE_00_11,

    // States for Prev_A_edge=0, Prev_B_edge=1
    STATE_01_00,
    STATE_01_01,
    STATE_01_10,
    STATE_01_11,

    // States for Prev_A_edge=1, Prev_B_edge=0
    STATE_10_00,
    STATE_10_01,
    STATE_10_10,
    STATE_10_11,

    // States for Prev_A_edge=1, Prev_B_edge=1
    STATE_11_00,
    STATE_11_01,
    STATE_11_10,
    STATE_11_11,

} State;

// Structure for ABZ configuration
typedef struct ABZ_Config_s {

    uint8_t channel;
    // Handle for PRUICSS instance
    PRUICSS_Handle icssHandle;
    /**< PRUICSS_Handle for icssg0 or icssg1 instance*/

    // PRUICSS core identifier
    uint32_t pru_slice;
    /**< PRUICSS core identifier
     * Check PRUICSS_PRU0 and check other available macros
    */

    // Base memory address for ABZ channel configuration
    uint32_t *baseMemAddr0; // icssHandle->hwAttrs->baseAddr + PRUICSS_DATARAM(PRUICSS_PRUx)
    /**< Base Memory Address for ABZ channel configuration */

    // Base memory address for ABZ channel configuration
    uint32_t *baseMemAddr1; // icssHandle->hwAttrs->baseAddr + PRUICSS_DATARAM(PRUICSS_PRUx)
    /**< Base Memory Address for ABZ channel configuration */

    // Pointer to ABZ interface structure
    ABZ_Handle *ABZInterface;
    /**< ABZ master memory interface structure */

    // Read and write pointers
    uint32_t *read_ptr;
    uint32_t *write_ptr;
    uint32_t mem_limit;
    // Offsets for read and write pointers
    uint32_t write_ptr_offset;
    uint32_t read_ptr_offset;

    // Edge detection variables
    uint32_t edges;
    uint32_t prev_ts; //*Previous time stamp*
    uint32_t cur_ts;  //*Current time stamp*

    // Speed and direction variables
    uint32_t speed;
    uint32_t delta_t;
    uint32_t iter;
    uint32_t iter1;

    // Edge status variables
    uint32_t A_cur_edge;
    uint32_t A_prev_edge;
    uint32_t B_prev_edge;
    uint32_t B_cur_edge;

    // Quadrature position counter
    uint32_t QPOSCOUNT;

    // Previous quadrature position
    int32_t prev_QPOS;

    // Edge status register
    int32_t A_B_EDGE_STATUS;  // 4 bit status for following fields-> [(Prev_A_Edge),(Prev_B_Edge),(Curr_A_Edge),(Curr_B_Edge)]


    // Direction variable
    int32_t direction;

    // Position variable
    uint32_t position;

    uint32_t *rev_intr;
    int32_t temp_qpos;
    // Qpos buffer
    uint32_t *position_base;
    uint32_t position_buffer[1000];

    // Phase-error shadow-compare (PRU-write-only counter, R5F-read-only)
    uint32_t *phase_err_base;
    uint32_t phase_err_count;
    uint32_t phase_err_count_last_seen;
    uint32_t phase_error_flag;
} ABZ_Config;


