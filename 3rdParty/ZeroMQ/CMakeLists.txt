ADD_LIBRARY (IMPORTED_LIBZMQ SHARED IMPORTED GLOBAL)

INCLUDE(ExternalProject)

ExternalProject_Add(LIBZMQ_BUILD
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}
        URL http://archive.org/download/zeromq_4.1.5/zeromq-4.1.5.tar.gz
        BUILD_COMMAND make -j4
        BUILD_IN_SOURCE ON
        INSTALL_DIR INSTALL
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> ${CONFIGURE_FLAGS} "CXX=${CMAKE_CXX_COMPILER}" "CC=${CMAKE_C_COMPILER}" "CFLAGS=${CMAKE_C_FLAGS}" "CXXFLAGS=${CMAKE_CXX_FLAGS} -Wno-error"
        UPDATE_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
        )

ExternalProject_Get_Property(LIBZMQ_BUILD INSTALL_DIR)

FILE (MAKE_DIRECTORY "${INSTALL_DIR}/lib" "${INSTALL_DIR}/include")
SET (LIBZMQ_INCLUDE_DIR "${INSTALL_DIR}/include")
SET (LIBZMQ_LIBRARY "${INSTALL_DIR}/lib/libzmq.so")

ADD_DEPENDENCIES (IMPORTED_LIBZMQ LIBZMQ_BUILD)

MESSAGE (STATUS "LIBZMQ IS ${LIBZMQ_LIBRARY}")
MESSAGE (STATUS "LIBZMQ_INCLUDE_DIR IS ${LIBZMQ_INCLUDE_DIR}")

SET_TARGET_PROPERTIES (IMPORTED_LIBZMQ PROPERTIES IMPORTED_LOCATION ${LIBZMQ_LIBRARY} INTERFACE_INCLUDE_DIRECTORIES ${LIBZMQ_INCLUDE_DIR})
