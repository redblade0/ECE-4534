################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
freertos/ticlang/%.o: ../freertos/ticlang/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccstheia151/ccs/tools/compiler/ti-cgt-armllvm_4.0.0.LTS/bin/tiarmclang.exe" -c -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O2 -I"C:/Users/bense/workspace_ccstheia/blink_led_LP_MSPM0G3507_freertos_ticlang" -I"C:/Users/bense/workspace_ccstheia/blink_led_LP_MSPM0G3507_freertos_ticlang/Debug" -I"C:/ti/mspm0_sdk_2_11_00_07/source/third_party/CMSIS/Core/Include" -I"C:/ti/mspm0_sdk_2_11_00_07/kernel/freertos/Source/include" -I"C:/ti/mspm0_sdk_2_11_00_07/source" -I"C:/ti/mspm0_sdk_2_11_00_07/kernel/freertos/Source/portable/TI_ARM_CLANG/ARM_CM0" -I"C:/ti/mspm0_sdk_2_11_00_07/source/ti/posix/ticlang" -I"C:/Users/bense/workspace_ccstheia/freertos_builds_LP_MSPM0G3507_release_ticlang" -D__MSPM0G3507__ -g -Wall -MMD -MP -MF"freertos/ticlang/$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


