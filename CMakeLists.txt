cmake_minimum_required(VERSION 2.8)
PROJECT(asio5-client)

aux_source_directory(. DIRSRCS)
ADD_EXECUTABLE(${PROJECT_NAME} ${DIRSRCS})
ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)

find_package(Boost
	1.54.0
	REQUIRED
	COMPONENTS system thread program_options log fiber
	)
if(Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
endif(Boost_FOUND)

