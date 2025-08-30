idf_component_register(
    INCLUDE_DIRS "src"
)

# Ensure subfolders like "fl" are resolvable as <fl/...>
target_include_directories(${COMPONENT_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
