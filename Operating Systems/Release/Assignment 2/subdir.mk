################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Assignment\ 2/main.c 

OBJS += \
./Assignment\ 2/main.o 

C_DEPS += \
./Assignment\ 2/main.d 


# Each subdirectory must supply rules for building sources it contributes
Assignment\ 2/main.o: ../Assignment\ 2/main.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Assignment 2/main.d" -MT"Assignment\ 2/main.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


