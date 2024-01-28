cmake_minimum_required(VERSION 3.21)
#Set c++ language standard
set(CMAKE_CXX_STANDARD 17)
project(metal_app)

# Enable objective c
enable_language(OBJCXX)

# Includes for metl definition
add_subdirectory(metal-cmake)  

# Include directories
include_directories(
	Src/
	metal-cmake/metal-cpp/
	metal-cmake/metal-cpp-extensions/
	.
)

# Set additioanl libs used in project
set(ALL_LIBS
		METAL_CPP
)

# Add exec
add_executable(MyApp
				Src/main.cpp
				Src/Renderer.cpp
				Src/MyMTKViewDelegate.cpp
				Src/MyAppDelegate.m
				Src/Shader.cpp)

# Link App with libraries				
target_link_libraries(MyApp
				${ALL_LIBS}
				"-framework Foundation"
			)