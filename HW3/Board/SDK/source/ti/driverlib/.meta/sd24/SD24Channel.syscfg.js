/*
 * Copyright (c) 2023 Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *  ======== SD24MSPM0.syscfg.js ========
 */

"use strict";

/* get Common /ti/driverlib utility functions */
let Common = system.getScript("/ti/driverlib/Common.js");

/************************* Profiles functions *******************************/

/************************* General functions **********************************/



/************************* Basic functions *******************************/
/* onChange functions for Basic Configuration */
let basicConfig = [
    // DL_SD24_setChannelPGAGain
    {
        name: "pgaGain",
        displayName: "PGA Gain",
        longDescription: `Programs the per-channel PGA gain register to scale ADC measurements. Each channel can have independent gain settings. Gain multiplies the input signal.`,
        default: "1",
        //  DL_SD24_CHANNEL_PGA_GAIN_
        options: [
            { name: "1",    displayName: "1"},
            { name: "2",    displayName: "2"},
            { name: "4",    displayName: "4"},
            { name: "8",    displayName: "8"},
            { name: "16",   displayName: "16"},
            { name: "32",   displayName: "32"},
            { name: "64",   displayName: "64"},
            { name: "128",  displayName: "128"},
        ]
    },
    //  DL_SD24_setChannelOffsetCalibration
    {
        name: "offsetCalibration",
        displayName: "Offset Calibration",
        longDescription: `Programs the per-channel offset calibration register (CHx_OCAL_MSB/LSB). This 24-bit value corrects DC offset errors in the ADC measurement.`,
        default: 0,
        isInteger: true,
        range: [0,0xFFFFFF]
    },
    // DL_SD24_setChannelGainCalibration
    {
        name: "gainCalibration",
        displayName: "Gain Calibration",
        longDescription: `Programs the per-channel gain calibration register (CHx_GCAL_MSB/LSB). This 24-bit value corrects gain errors in the ADC measurement. Range: 0.0 to 2.0·(2^24 – 1) / 2^24.`,
        default: 0,
        isInteger: true,
        range: [0,0xFFFFFF]
    },
    // DL_SD24_setChannelInputMux
    {
        name: "inputMux",
        displayName: "Input Multiplexer Selection",
        longDescription: `Programs the per-channel input multiplexer (CHx_CFG MUX bits [1:0]). Selects which signals the channel measures: differential inputs, shorted inputs, or DC test signals.`,
        default: "DIFFERENTIAL",
        //  DL_SD24_CHANNEL_INPUT_MUX_
        options: [
            { name: "DIFFERENTIAL",     displayName: "Differential (AIN0P and AIN0N)"},
            { name: "SHORTED",          displayName: "Shorted Inputs"},
            { name: "POS_DC_TEST",      displayName: "Positive DC Test Signal"},
            { name: "NEG_DC_TEST",      displayName: "Negative DC Test Signal"},
        ]
    },
    // DL_SD24_setChannelPhase
    {
        name: "phase",
        displayName: "Phase Adjustment",
        longDescription: `Programs the per-channel phase adjustment (CHx_CFG PHASE bits [15:6]). Allows fine-grained alignment of channel sampling relative to other channels. Range: 0–1023.`,
        default: 0,
        isInteger: true,
        range: [0, 1023]
    },
    // DL_SD24_setChannelDCBlockDisable
    {
        name: "dcBlockDisable",
        displayName: "Disable DC Block Filter",
        longDescription: `Programs the per-channel DC block disable setting (CHx_CFG DCBLK_DIS bit [5]). When true, the DC blocking high-pass filter for this channel is disabled.`,
        default: false,
    },
]

// let config = [
//     {
//         name: "GROUP_BASIC",
//         displayName: "Basic Configuration",
//         description: "",
//         collapsed: false,
//         config: basicConfig
//     }
// ]
let config = basicConfig;




/*
 *  ======== validate ========
 *  Validate this inst's configuration
 *
 *  param inst       - module instance to be validated
 *  param validation - object to hold detected validation issues
 */
function validate(inst, validation)
{

}

/*
 *  ======== devSpecific ========
 *  Device-specific extensions to be added to base module configuration
 */
let devSpecific = {

    config: config,

    validate: validate,
};

/*
 *  ======== exports ========
 *  Export device-specific extensions to base exports
 */
exports = {
    /* required function, called by base module module */
    config: config
};
