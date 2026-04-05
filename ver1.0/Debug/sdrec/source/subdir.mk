################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../sdrec/source/sdrec_card_port.c \
../sdrec/source/sdrec_crc32.c \
../sdrec/source/sdrec_layout_v3.c \
../sdrec/source/sdrec_pipe.c \
../sdrec/source/sdrec_runtime.c \
../sdrec/source/sdrec_sink.c \
../sdrec/source/sdrec_source_api.c \
../sdrec/source/sdrec_source_dummy_cpu.c \
../sdrec/source/sdrec_source_wave_dma.c \
../sdrec/source/sdrec_source_wave_table.c \
../sdrec/source/sdrec_source_wave_table_data.c 

OBJS += \
./sdrec/source/sdrec_card_port.o \
./sdrec/source/sdrec_crc32.o \
./sdrec/source/sdrec_layout_v3.o \
./sdrec/source/sdrec_pipe.o \
./sdrec/source/sdrec_runtime.o \
./sdrec/source/sdrec_sink.o \
./sdrec/source/sdrec_source_api.o \
./sdrec/source/sdrec_source_dummy_cpu.o \
./sdrec/source/sdrec_source_wave_dma.o \
./sdrec/source/sdrec_source_wave_table.o \
./sdrec/source/sdrec_source_wave_table_data.o 

C_DEPS += \
./sdrec/source/sdrec_card_port.d \
./sdrec/source/sdrec_crc32.d \
./sdrec/source/sdrec_layout_v3.d \
./sdrec/source/sdrec_pipe.d \
./sdrec/source/sdrec_runtime.d \
./sdrec/source/sdrec_sink.d \
./sdrec/source/sdrec_source_api.d \
./sdrec/source/sdrec_source_dummy_cpu.d \
./sdrec/source/sdrec_source_wave_dma.d \
./sdrec/source/sdrec_source_wave_table.d \
./sdrec/source/sdrec_source_wave_table_data.d 


# Each subdirectory must supply rules for building sources it contributes
sdrec/source/%.o sdrec/source/%.su sdrec/source/%.cyclo: ../sdrec/source/%.c sdrec/source/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I"C:/Users/YOOCHAN/Desktop/ongoing_projects/ARES2026/RAW-SDIO/SDIO_RAW_filesystem/ver1.0/sdrec/include" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-sdrec-2f-source

clean-sdrec-2f-source:
	-$(RM) ./sdrec/source/sdrec_card_port.cyclo ./sdrec/source/sdrec_card_port.d ./sdrec/source/sdrec_card_port.o ./sdrec/source/sdrec_card_port.su ./sdrec/source/sdrec_crc32.cyclo ./sdrec/source/sdrec_crc32.d ./sdrec/source/sdrec_crc32.o ./sdrec/source/sdrec_crc32.su ./sdrec/source/sdrec_layout_v3.cyclo ./sdrec/source/sdrec_layout_v3.d ./sdrec/source/sdrec_layout_v3.o ./sdrec/source/sdrec_layout_v3.su ./sdrec/source/sdrec_pipe.cyclo ./sdrec/source/sdrec_pipe.d ./sdrec/source/sdrec_pipe.o ./sdrec/source/sdrec_pipe.su ./sdrec/source/sdrec_runtime.cyclo ./sdrec/source/sdrec_runtime.d ./sdrec/source/sdrec_runtime.o ./sdrec/source/sdrec_runtime.su ./sdrec/source/sdrec_sink.cyclo ./sdrec/source/sdrec_sink.d ./sdrec/source/sdrec_sink.o ./sdrec/source/sdrec_sink.su ./sdrec/source/sdrec_source_api.cyclo ./sdrec/source/sdrec_source_api.d ./sdrec/source/sdrec_source_api.o ./sdrec/source/sdrec_source_api.su ./sdrec/source/sdrec_source_dummy_cpu.cyclo ./sdrec/source/sdrec_source_dummy_cpu.d ./sdrec/source/sdrec_source_dummy_cpu.o ./sdrec/source/sdrec_source_dummy_cpu.su ./sdrec/source/sdrec_source_wave_dma.cyclo ./sdrec/source/sdrec_source_wave_dma.d ./sdrec/source/sdrec_source_wave_dma.o ./sdrec/source/sdrec_source_wave_dma.su ./sdrec/source/sdrec_source_wave_table.cyclo ./sdrec/source/sdrec_source_wave_table.d ./sdrec/source/sdrec_source_wave_table.o ./sdrec/source/sdrec_source_wave_table.su ./sdrec/source/sdrec_source_wave_table_data.cyclo ./sdrec/source/sdrec_source_wave_table_data.d ./sdrec/source/sdrec_source_wave_table_data.o ./sdrec/source/sdrec_source_wave_table_data.su

.PHONY: clean-sdrec-2f-source

