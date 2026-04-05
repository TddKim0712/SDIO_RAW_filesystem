################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../0318/Core/Startup/startup_stm32f411retx.s 

OBJS += \
./0318/Core/Startup/startup_stm32f411retx.o 

S_DEPS += \
./0318/Core/Startup/startup_stm32f411retx.d 


# Each subdirectory must supply rules for building sources it contributes
0318/Core/Startup/%.o: ../0318/Core/Startup/%.s 0318/Core/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -DDEBUG -c -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-0318-2f-Core-2f-Startup

clean-0318-2f-Core-2f-Startup:
	-$(RM) ./0318/Core/Startup/startup_stm32f411retx.d ./0318/Core/Startup/startup_stm32f411retx.o

.PHONY: clean-0318-2f-Core-2f-Startup

