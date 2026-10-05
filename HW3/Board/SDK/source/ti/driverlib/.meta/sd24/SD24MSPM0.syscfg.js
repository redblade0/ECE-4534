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
    // DL_SD24_setDrdySel
    {
        name: "drdySel",
        displayName: "Data Ready Signal Selection",
        longDescription: `Programs the MODE register DRDY_SEL bits [9:8] to determine which channel(s) trigger the DRDY signal. This affects when data ready interrupts are generated.`,
        default: "LAGGING",
        //  DL_SD24_MODE_DRDY_SEL_
        options: [
            { name: "LAGGING",      displayName: "Most Lagging Enabled Channel"},
            { name: "OR",           displayName: "Logic OR of All Enabled Channels"},
            { name: "LEADING",     displayName: "Most Leading Enabled Channel"},
        ]
    },
    // DL_SD24_enableExternalReference
    {
        name: "enableExternalReference",
        displayName: "Enable External Reference",
        longDescription: `Enables or disables the external reference feature (CLOCK register EXTREF_EN bit [6]). When enabled, the SD24 uses an external voltage reference instead of the internal one.`,
        default: false,
    },
];

let currentDetectConfig = [
    // DL_SD24_enableCurrentDetect
    {
        name: "enableCurrentDetect",
        displayName: "Enable Current Detect",
        longDescription: `Enables or disables the current detect feature (CFG register CD_EN bit [0]). Current detect monitors channel outputs against a threshold to flag overcurrent conditions.`,
        default: false,
        hidden: true,
    },
    // DL_SD24_setCurrentDetectConfig - currentDetectNumberThresholds
    {
        name: "cdNumberThresholds",
        displayName: "Number of Exceeded Thresholds",
        longDescription: `Number of channel thresholds that must be exceeded to trigger a current detect event (CFG register bits [4:2]). Range: 0–7.`,
        default: 0,
        isInteger: true,
        range: [0, 7],
        hidden: true,
    },
    // DL_SD24_setCurrentDetectConfig - currentDetectMeasurementLength
    {
        name: "cdMeasurementLength",
        displayName: "Measurement Length",
        longDescription: `Measurement length in conversion periods for the current detect function (CFG register bits [7:5]). Range: 0–7.`,
        default: 0,
        isInteger: true,
        range: [0, 7],
        hidden: true,
    },
    // DL_SD24_setCurrentDetectConfig - currentDetectChannelSelection
    {
        name: "cdChannelSelection",
        displayName: "Current Detect Channel Selection",
        longDescription: `Selects whether any single channel or all channels must exceed the threshold to trigger a current detect event (CFG register bit [8]).`,
        default: "ANY_CHANNEL",
        // DL_SD24_CURRENT_DETECT_CHANNEL_
        options: [
            { name: "ANY_CHANNEL",   displayName: "Any Channel"},
            { name: "ALL_CHANNELS",  displayName: "All Channels"},
        ],
        hidden: true,
    },
    // DL_SD24_setCurrentDetectThreshold
    {
        name: "currentDetectThreshold",
        displayName: "Current Detect Threshold",
        longDescription: `24-bit threshold value for current detect. The value is split across THRSHLD_MSB (bits [23:8]) and THRSHLD_LSB (bits [15:8]). Range: 0–0xFFFFFF.`,
        default: 0,
        isInteger: true,
        range: [0, 0xFFFFFF],
        hidden: true,
    },
    // DL_SD24_setDCBlockFilterValue
    {
        name: "dcBlockFilterValue",
        displayName: "DC Block Filter Value",
        longDescription: `DC block filter setting (THRSHLD_LSB DCBLOCK bits [3:0]). Controls the corner frequency of the DC blocking high-pass filter. Range: 0–15.`,
        default: 0,
        isInteger: true,
        range: [0, 15],
        hidden: true,
    },
];

let globalChopConfig = [
    // DL_SD24_enableGlobalChop
    {
        name: "enableGlobalChop",
        displayName: "Enable Global Chop",
        longDescription: `Enables or disables the global chop feature (CFG register GC_EN bit [9]). Global chopping reduces offset errors by periodically inverting the input signal polarity.`,
        default: false,
        hidden: true,
    },
    // DL_SD24_setGlobalChopDelay
    {
        name: "globalChopDelay",
        displayName: "Global Chop Delay",
        longDescription: `Number of modulator clock periods to wait before beginning a measurement after a chop event (CFG register GC_DLY bits [13:10]).`,
        default: "16",
        // DL_SD24_GLOBAL_CHOP_DELAY_
        options: [
            { name: "2",     displayName: "2 Modulator Clock Periods"},
            { name: "4",     displayName: "4 Modulator Clock Periods"},
            { name: "8",     displayName: "8 Modulator Clock Periods"},
            { name: "16",    displayName: "16 Modulator Clock Periods (Default)"},
            { name: "32",    displayName: "32 Modulator Clock Periods"},
            { name: "64",    displayName: "64 Modulator Clock Periods"},
            { name: "128",   displayName: "128 Modulator Clock Periods"},
            { name: "256",   displayName: "256 Modulator Clock Periods"},
            { name: "512",   displayName: "512 Modulator Clock Periods"},
            { name: "1024",  displayName: "1024 Modulator Clock Periods"},
            { name: "2048",  displayName: "2048 Modulator Clock Periods"},
            { name: "4096",  displayName: "4096 Modulator Clock Periods"},
            { name: "8192",  displayName: "8192 Modulator Clock Periods"},
            { name: "16384", displayName: "16384 Modulator Clock Periods"},
            { name: "32768", displayName: "32768 Modulator Clock Periods"},
            { name: "65536", displayName: "65536 Modulator Clock Periods"},
        ],
        hidden: true,
    },
];

let channelConfig = [
    //  DL_SD24_setChannelEnableMask
    {
        name: "enabledChannels",
        displayName: "Enable Channels",
        longDescription: `Programs the CLOCK register channel enable bits [15:8]. Selected channels will be enabled and channel-specific configuration modules will be created. All channels are enabled by default.`,
        default: ["0", "1", "2", "3", "4", "5", "6"],
        minSelections: 0,
        options: [
            { name: "0", displayName: "Channel 0"},
            { name: "1", displayName: "Channel 1"},
            { name: "2", displayName: "Channel 2"},
            { name: "3", displayName: "Channel 3"},
            { name: "4", displayName: "Channel 4"},
            { name: "5", displayName: "Channel 5"},
            { name: "6", displayName: "Channel 6"},
        ]
    }
];

let clockConfig = [
    // DL_SD24_ClockConfig
    // DL_SD24_ClockOSR
    {
        name: "clockOSR",
        displayName: "Oversampling Ratio",
        longDescription: `Oversampling Ratio (OSR) selection - determines the trade-off between conversion speed and noise. Higher OSR values provide better noise performance but slower conversion speed.`,
        default: "128",
        options: [
            // DL_SD24_CLOCK_OSR_
            { name: "128",      displayName: "128"},
            { name: "256",      displayName: "256"},
            { name: "512",      displayName: "512"},
            { name: "1024",     displayName: "1024"},
            { name: "2048",     displayName: "2048"},
            { name: "4096",     displayName: "4096"},
            { name: "8192",     displayName: "8192"},
            { name: "16256",    displayName: "16256"},
        ]
    },
    // DL_SD24_ClockPowerMode
    {
        name: "clockPowerMode",
        displayName: "Clock Power Mode",
        longDescription: `Power mode selection - controls conversion speed and power consumption. Higher resolution modes consume more power but provide better performance.`,
        default: "HIGH_RES",
        options: [
            // DL_SD24_CLOCK_POWER_
            { name: "VERY_LOW",  displayName: "Very Low Power"},
            { name: "LOW",       displayName: "Low Power"},
            { name: "HIGH_RES",  displayName: "High Resolution"},
        ]
    }
];

/* PROFILES CONFIGURATION */
let config = [
    /****** PROFILES CONFIGURATION *******/
    {
        name: "GROUP_BASIC",
        displayName: "Basic Configuration",
        description: "",
        collapsed: false,
        config: basicConfig
    },
    {
        name: "GROUP_CLOCK",
        displayName: "Clock Configuration",
        description: "",
        collapsed: false,
        config: clockConfig
    },
        {
        name: "GROUP_ADVANCED",
        displayName: "Advanced Configuration",
        description: "",
        collapsed: false,
        config: [
            {
                name: "configureCurrentDetect",
                displayName: "Configure Current Detect Settings",
                default: false,
                onChange: (inst,ui)=>{
                    if(inst.configureCurrentDetect){
                        system.utils.showGroupConfig("GROUP_CURRENT_DETECT", inst,ui);
                    }
                    else{
                        system.utils.hideGroupConfig("GROUP_CURRENT_DETECT", inst,ui);
                    }

                }
            },
            {
                name: "GROUP_CURRENT_DETECT",
                displayName: "Current Detect Configuration",
                description: "",
                collapsed: false,
                config: currentDetectConfig
            },
            {
                name: "configureGlobalChop",
                displayName: "Configure Global Chop Settings",
                default: false,
                onChange: (inst,ui)=>{
                    if(inst.configureGlobalChop){
                        system.utils.showGroupConfig("GROUP_GLOBAL_CHOP", inst,ui);
                    }
                    else{
                        system.utils.hideGroupConfig("GROUP_GLOBAL_CHOP", inst,ui);
                    }

                }
            },
            {
                name: "GROUP_GLOBAL_CHOP",
                displayName: "Global Chop Configuration",
                description: "",
                collapsed: false,
                config: globalChopConfig
            },
        ]
    },
    {
        name: "GROUP_CHANNELS",
        displayName: "Channel Configuration",
        description: "",
        collapsed: false,
        config: channelConfig
    },
]





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

function pinmuxRequirements(inst){
    let resources = [
        { name: "agndPin",  displayName: "Analog Ground",        interfaceNames: ["AGND"]  },
        { name: "avddPin",  displayName: "Analog Power Supply",   interfaceNames: ["AVDD"]  },
        { name: "capPin",   displayName: "Capacitor Pin",         interfaceNames: ["CAP"]   },
        { name: "refinPin", displayName: "Reference Input",       interfaceNames: ["REFIN"] },
    ];
    for (let ch = 0; ch <= 6; ch++) {
        if (inst.enabledChannels.includes(String(ch))) {
            resources.push(
                { name: "ain"+ch+"nPin", displayName: "Analog Input "+ch+" Negative", interfaceNames: ["AIN"+ch+"N"] },
                { name: "ain"+ch+"pPin", displayName: "Analog Input "+ch+" Positive", interfaceNames: ["AIN"+ch+"P"] },
            );
        }
    }

    let sd24 = {
        name          : "peripheral",
        displayName   : "SD24 Peripheral",
        interfaceName : "SD24",
        resources     : resources,
        signalTypes   : {
            agndPin:  ["AGND"],
            ain0nPin: ["AIN0N"],
            ain0pPin: ["AIN0P"],
            ain1nPin: ["AIN1N"],
            ain1pPin: ["AIN1P"],
            ain2nPin: ["AIN2N"],
            ain2pPin: ["AIN2P"],
            ain3nPin: ["AIN3N"],
            ain3pPin: ["AIN3P"],
            ain4nPin: ["AIN4N"],
            ain4pPin: ["AIN4P"],
            ain5nPin: ["AIN5N"],
            ain5pPin: ["AIN5P"],
            ain6nPin: ["AIN6N"],
            ain6pPin: ["AIN6P"],
            avddPin:  ["AVDD"],
            capPin:   ["CAP"],
            refinPin: ["REFIN"],
        },
    };
    return [sd24];
}

function moduleInstances(inst){
    let modInstances = [];
    let firstCollapse = false;
      for(let i in inst.enabledChannels){
        let chanIdx = inst.enabledChannels[i];
        let str = "SD24_CHANNEL_"+ chanIdx;
        let displayStr = "SD24 Channel "+ chanIdx;
        let desc = "Channel-Specific Configurables for SD24 Channel "+chanIdx;
        let longDescription = "";
        let mod = {
            name: str,
            displayName: displayStr,
            description: desc,
            longDescription: longDescription,
            group: "GROUP_CHANNELS",
            collapsed: firstCollapse,
            moduleName: "/ti/driverlib/sd24/SD24Channel",
            hidden: false,
            args: {
                // passedCount: inst.timerCount,
            }
        };
        modInstances.push(mod);
        firstCollapse = true; // subsequent modules are not collapsed
    }
    return modInstances;
}

/*
 *  ======== devSpecific ========
 *  Device-specific extensions to be added to base module configuration
 */
let devSpecific = {

    config: config,

    validate: validate,

    pinmuxRequirements: pinmuxRequirements,

    moduleInstances: moduleInstances,

    /* override device-specific templates */
    templates: {
        boardc: "/ti/driverlib/sd24/SD24.Board.c.xdt",
        boardh: "/ti/driverlib/sd24/SD24.Board.h.xdt",
        Call: true,
        Reset: false,
        Power: false,
        GPIO: false,
        Function: true

    },

};

/*
 *  ======== extend ========
 *  Extends a base exports object to include any device specifics
 *
 *  This function is invoked by the generic module module to
 *  allow us to augment/override as needed for the MSPM0.
 */
function extend(base)
{
    /* merge and overwrite base module attributes */
    Object.assign(base, devSpecific);

    return (base);
}

/*
 *  ======== exports ========
 *  Export device-specific extensions to base exports
 */
exports = {
    /* required function, called by base module module */
    extend: extend
};
