#[[
  Find Pqxx
  This module finds an installed pqxx package.

  It sets the following variables:
    PQXX_FOUND       - Set to false, or undefined, if nlohmann/json isn't found.
    PQXX_INCLUDE_DIR - The pqxx include directory.
    PQXX             - The pqxx library to link against
]]

find_path(PQXX_INCLUDE_DIR pqxx/pqxx)
find_library(PQXX_LIBRARY NAMES pqxx)

if(PQXX_INCLUDE_DIR AND PQXX_LIBRARY)
  set(PQXX_FOUND TRUE)
endif(PQXX_INCLUDE_DIR AND PQXX_LIBRARY)

if(PQXX_FOUND)

  # show which nlohmann/json was found only if not quiet
  if(NOT PQXX_FIND_QUIETLY)
    message(STATUS "Found pqxx: ${PQXX_INCLUDE_DIR}")
  endif(NOT PQXX_FIND_QUIETLY)

else(PQXX_FOUND)

  # fatal error if pqxx is required but not found
  if(PQXX_FIND_REQUIRED)
    message(FATAL_ERROR "Could not find pqxx")
  endif(PQXX_FIND_REQUIRED)

endif(PQXX_FOUND)
