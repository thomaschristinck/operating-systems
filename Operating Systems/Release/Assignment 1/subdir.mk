################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Assignment\ 1/jobs.c \
../Assignment\ 1/main.c 

OBJS += \
./Assignment\ 1/jobs.o \
./Assignment\ 1/main.o 

C_DEPS += \
./Assignment\ 1/jobs.d \
./Assignment\ 1/main.d 


# Each subdirectory must supply rules for building sources it contributes
Assignment\ 1/jobs.o: ../Assignment\ 1/jobs.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Assignment 1/jobs.d" -MT"Assignment\ 1/jobs.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Assignment\ 1/main.o: ../Assignment\ 1/main.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"Assignment 1/main.d" -MT"Assignment\ 1/main.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


