cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)

project(ModbusTCP)

ADD_LIBRARY( TcpModbusLib STATIC
				ModbusTCP.h
				ModbusTCP.cpp
				Socket/Socket.hxx
				Socket/Socket.cxx
				Socket/TCP_Socket.hxx
				Socket/TCP_Socket.cxx
				)

#file(GLOB SOURCES "*.cpp" "*.h" "./Socket/*.cxx" "./Socket/*.hxx")

#add_executable(${PROJECT_NAME} ${SOURCES})

SET( APP_EXE TcpModbusTest )
ADD_EXECUTABLE( ${APP_EXE}
                main.cpp )
TARGET_LINK_LIBRARIES( ${APP_EXE}
                       TcpModbusLib )