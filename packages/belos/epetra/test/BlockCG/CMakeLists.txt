

TRIBITS_ADD_EXECUTABLE_AND_TEST(
  pseudo_cg_indefinite
  SOURCES test_pseudo_cg_indefinite.cpp   
  COMM serial mpi
  ARGS
    "--debug --verbose --tol=1e-5"
  STANDARD_PASS_OUTPUT
  )

ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_Triutils)
IF (${PACKAGE_NAME}_ENABLE_Triutils)
    
  TRIBITS_ADD_EXECUTABLE_AND_TEST(
    bl_cg_hb
    SOURCES test_bl_cg_hb.cpp 
    COMM serial mpi
    ARGS
      "--verbose --filename=bcsstk14.hb"
      "--verbose --filename=bcsstk14.hb --use-single-red"
    STANDARD_PASS_OUTPUT 
    )

  TRIBITS_ADD_EXECUTABLE_AND_TEST(
    pseudo_cg_hb
    SOURCES test_pseudo_cg_hb.cpp 
    COMM serial mpi
    ARGS
      "--verbose --filename=bcsstk14.hb"
    STANDARD_PASS_OUTPUT 
    )

  TRIBITS_ADD_EXECUTABLE_AND_TEST(
    resolve_cg_hb
    SOURCES test_resolve_cg_hb.cpp 
    COMM serial mpi
    ARGS
      "--verbose --filename=bcsstk14.hb"
    STANDARD_PASS_OUTPUT 
    )
    
  TRIBITS_ADD_EXECUTABLE_AND_TEST(
    bl_cg_set_res
    SOURCES test_bl_cg_set_res.cpp 
    COMM serial mpi
    ARGS
      "--verbose --filename=bcsstk14.hb"
    STANDARD_PASS_OUTPUT 
    )

  TRIBITS_COPY_FILES_TO_BINARY_DIR(CopyTestBlockCGFiles
    SOURCE_DIR ${Belos_SOURCE_DIR}/testmatrices
    SOURCE_FILES bcsstk14.hb
    EXEDEPS bl_cg_hb
    )

  ASSERT_DEFINED(${PACKAGE_NAME}_ENABLE_Ifpack)
  IF(${PACKAGE_NAME}_ENABLE_Ifpack)

    TRIBITS_ADD_EXECUTABLE_AND_TEST(
      bl_pcg_hb
      SOURCES test_bl_pcg_hb.cpp 
      COMM serial mpi
      ARGS
        "--verbose --filename=bcsstk14.hb --left-prec --tol=1e-8 --num-rhs=5 --block-size=3 --max-iters=100"
        "--verbose --filename=bcsstk14.hb --right-prec --tol=1e-8 --num-rhs=5 --block-size=3 --max-iters=100"
        "--verbose --filename=bcsstk14.hb --tol=1e-8 --num-rhs=1 --block-size=1 --max-iters=110 --use-single-red"
        "--verbose --filename=bcsstk14.hb --tol=1e-8 --num-rhs=1 --block-size=1 --max-iters=110"
      STANDARD_PASS_OUTPUT 
      )
      
    TRIBITS_ADD_EXECUTABLE_AND_TEST(
      bl_pcg_set_res
      SOURCES test_bl_pcg_set_res.cpp 
      COMM serial mpi
      ARGS
        "--verbose --filename=bcsstk14.hb --left-prec --tol=1e-8 --num-rhs=5 --block-size=3 --max-iters=100"
        "--verbose --filename=bcsstk14.hb --right-prec --tol=1e-8 --num-rhs=5 --block-size=3 --max-iters=100"
      STANDARD_PASS_OUTPUT 
      )

    TRIBITS_ADD_EXECUTABLE_AND_TEST(
      pseudo_pcg_hb
      SOURCES test_pseudo_pcg_hb.cpp 
      COMM serial mpi
      ARGS
        "--verbose --filename=bcsstk14.hb --left-prec --tol=1e-8 --max-iters=110"
        "--verbose --filename=bcsstk14.hb --left-prec --tol=1e-8 --num-rhs=5 --max-iters=110"
        "--verbose --filename=bcsstk14.hb --right-prec --tol=1e-8 --num-rhs=5 --max-iters=110"
        "--verbose --filename=bcsstk14.hb --left-prec --tol=1e-8 --num-rhs=1 --max-iters=110"
      STANDARD_PASS_OUTPUT 
      )

    TRIBITS_ADD_EXECUTABLE_AND_TEST(
      pseudo_stochastic_pcg_hb
      SOURCES test_pseudo_stochastic_pcg_hb.cpp 
      COMM serial mpi
      ARGS
        "--verbose --filename=bcsstk14.hb --left-prec --tol=1e-8 --num-rhs=5 --max-iters=110"
        "--verbose --filename=bcsstk14.hb --right-prec --tol=1e-8 --num-rhs=5 --max-iters=110"
      STANDARD_PASS_OUTPUT 
      )



  ENDIF(${PACKAGE_NAME}_ENABLE_Ifpack)

ENDIF(${PACKAGE_NAME}_ENABLE_Triutils)
