# Copyright 2002 - 2008, 2010, 2011 National Technology Engineering
# Solutions of Sandia, LLC (NTESS). Under the terms of Contract
# DE-NA0003525 with NTESS, the U.S. Government retains certain rights
# in this software.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
# 
#     * Neither the name of NTESS nor the names of its contributors
#       may be used to endorse or promote products derived from this
#       software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 

FILE(GLOB SOURCES CONFIGURE_DEPENDS *.cpp)

IF(NOT ${PROJECT_NAME}_ENABLE_STKTransferUtil)
LIST(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/howToUseLeastSquaresInterpolation.cpp")
LIST(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/howToUseSimpleTransfer.cpp")
ENDIF()

if(HAVE_STK_Trilinos)
  TRIBITS_ADD_EXECUTABLE_AND_TEST(stk_transfer_doc_tests
    SOURCES ${SOURCES}
    TESTONLYLIBS stk_unit_main
    ARGS "--gtest_filter=StkTransferHowTo.useCopyTransfer"
    COMM mpi
    NUM_MPI_PROCS 2
    NOEXEPREFIX
  )
else()
  add_executable(stk_transfer_doc_tests ${SOURCES})
  if(${PROJECT_NAME}_ENABLE_STKTransferUtil)
    target_link_libraries(stk_transfer_doc_tests stk_transfer_util)
  endif()
  target_link_libraries(stk_transfer_doc_tests stk_transfer_lib)
  target_link_libraries(stk_transfer_doc_tests stk_coupling)
  target_link_libraries(stk_transfer_doc_tests stk_unit_test_utils)
  target_link_libraries(stk_transfer_doc_tests stk_util_parallel)
  target_link_libraries(stk_transfer_doc_tests stk_unit_main)


  INSTALL(TARGETS stk_transfer_doc_tests DESTINATION ${STK_INSTALL_BINDIR})

  FOREACH(NUMPROCS 1 2)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/np${NUMPROCS})
    add_test(NAME "stk_transfer_doc_tests_np${NUMPROCS}"
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/np${NUMPROCS} 
             COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${NUMPROCS} ${CMAKE_CURRENT_BINARY_DIR}/stk_transfer_doc_tests 
             --gtest_filter=StkTransferHowTo.useCopyTransfer)
    set_property(TEST "stk_transfer_doc_tests_np${NUMPROCS}" PROPERTY PROCESSORS ${NUMPROCS})
  ENDFOREACH()

endif()

