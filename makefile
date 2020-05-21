RM := rm -rf

OPENCV_INCLUDE := -I$(OPENCV_PATH)/include
MXNET_INCLUDE := -I$(MXNET_PATH)/include

# Add inputs and outputs from these tool invocations to the build variables
CPP_SRCS += \
./gsFaceDetectHttpServer.cpp \
./HttpServer.cpp \
./MatData.cpp \
./mongoose.c \
./base64.cpp \
./ConnJDBC.cpp \
./logger.cpp

OBJS += \
./gsFaceDetectHttpServer.o \
./HttpServer.o \
./MatData.o \
./mongoose.o \
./base64.o \
./ConnJDBC.o \
./logger.o

CPP_DEPS += \
./gsFaceDetectHttpServer.d \
./HttpServer.d \
./MatData.d \
./ConnJDBC.d \
./base64.d \
./mongoose.d \
./logger.d

# All Target
all: gsFaceDetectHttpServer

LIBS := -lmysqlcppconn -lopencv_imgcodecs -lgsFaceDetectSDK -lopencv_videoio -lpthread -lmxnet -lmklml_intel -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_flann -lopencv_features2d -lomp -ljsoncpp

#LIBS := -lopencv_imgcodecs -lgsFaceMotDeepSort -ltensorflow_cc -ltensorflow_framework -lgsFaceDetectSDK -lopencv_videoio -lpthread -lmxnet -lmklml_intel -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_flann -lopencv_features2d -lomp

# Each subdirectory must supply rules for building sources it contributes
%.o: ./%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	#g++ -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<" $(OPENCV_INCLUDE) $(MXNET_INCLUDE)
	g++ -I/usr/local/include -I/opt/lib_ai/jsoncpp/include -I/opt/lib_ai/mysql_conn_8.0.19/include -I/opt/ai/gsFaceDetectSDK -I/opt/lib_ai/eigen/include/eigen3 -I/opt/lib_ai/mxnet/include -I/opt/lib_ai/boost/include -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

# Tool invocations
gsFaceDetectHttpServer: $(OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	 g++ -L/usr/local/lib -L/opt/lib_ai/jsoncpp/lib -L/opt/lib_ai/mysql_conn_8.0.19/lib -L/opt/ai/gsFaceDetectSDK -L/opt/lib_ai/mxnet/mklml_20190502/lib -L/opt/lib_ai/mxnet/lib -o "gsFaceDetectHttpServer" $(OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS) $(CPP_DEPS) gsFaceDetectHttpServer 
	-@echo ' '
