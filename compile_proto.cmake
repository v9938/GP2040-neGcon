function (compile_proto)
	find_package(Python3 REQUIRED COMPONENTS Interpreter)
	option(GP2040_USE_SYSTEM_PYTHON "Use an existing Python with nanopb generator dependencies" OFF)

	set(VENV ${CMAKE_CURRENT_BINARY_DIR}/venv)
	set(VENV_FILE ${VENV}/environment.txt)
	if(CMAKE_HOST_WIN32)
		set(VENV_BIN_DIR ${VENV}/Scripts)
	else()
		set(VENV_BIN_DIR ${VENV}/bin)
	endif()

	if(GP2040_USE_SYSTEM_PYTHON)
		execute_process(COMMAND ${Python3_EXECUTABLE} -c "import google.protobuf, grpc_tools.protoc"
			RESULT_VARIABLE PYTHON_DEPENDENCIES_RESULT)
		if(NOT PYTHON_DEPENDENCIES_RESULT EQUAL 0)
			message(FATAL_ERROR "Install lib/nanopb/extra/requirements.txt into the selected Python first")
		endif()
		set(PROTO_PYTHON ${Python3_EXECUTABLE})
		set(PROTO_ENV_DEPENDENCY ${CMAKE_SOURCE_DIR}/lib/nanopb/extra/requirements.txt)
	else()
	add_custom_command(
		DEPENDS ${CMAKE_SOURCE_DIR}/lib/nanopb/extra/requirements.txt
		COMMAND ${Python3_EXECUTABLE} -m venv ${VENV}
		COMMAND ${VENV_BIN_DIR}/pip --disable-pip-version-check install -r ${CMAKE_SOURCE_DIR}/lib/nanopb/extra/requirements.txt
		COMMAND ${VENV_BIN_DIR}/pip freeze > ${VENV_FILE}
		OUTPUT ${VENV_FILE}
		COMMENT "Setting up Python Virtual Environment"
	)
	set(PROTO_PYTHON ${VENV_BIN_DIR}/python)
	set(PROTO_ENV_DEPENDENCY ${VENV_FILE})
	endif()

	set(NANOPB_GENERATOR ${CMAKE_SOURCE_DIR}/lib/nanopb/generator/nanopb_generator.py)
	set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
	set(PROTO_OUTPUT_DIR ${PROTO_OUTPUT_DIR} PARENT_SCOPE)

	add_custom_command(
		DEPENDS ${PROTO_ENV_DEPENDENCY} ${NANOPB_GENERATOR} ${CMAKE_SOURCE_DIR}/proto/enums.proto ${CMAKE_SOURCE_DIR}/proto/config.proto ${CMAKE_SOURCE_DIR}/lib/nanopb/generator/proto/nanopb.proto
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTO_OUTPUT_DIR}
		COMMAND ${PROTO_PYTHON} ${NANOPB_GENERATOR}
			-q
			-D ${PROTO_OUTPUT_DIR}
			-I ${CMAKE_SOURCE_DIR}/proto
			-I ${CMAKE_SOURCE_DIR}/lib/nanopb/generator/proto
			${CMAKE_SOURCE_DIR}/proto/enums.proto
		COMMAND ${PROTO_PYTHON} ${NANOPB_GENERATOR}
			-q
			-D ${PROTO_OUTPUT_DIR}
			-I ${CMAKE_SOURCE_DIR}/proto
			-I ${CMAKE_SOURCE_DIR}/lib/nanopb/generator/proto
			${CMAKE_SOURCE_DIR}/proto/config.proto
		OUTPUT ${PROTO_OUTPUT_DIR}/config.pb.c ${PROTO_OUTPUT_DIR}/config.pb.h ${PROTO_OUTPUT_DIR}/enums.pb.c ${PROTO_OUTPUT_DIR}/enums.pb.h
		COMMENT "Compiling enums.proto and config.proto"
	)
endfunction()
